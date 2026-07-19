#include <chrono>
#include <filesystem>
#include <fstream>
#include <functional>
#include <iostream>
#include <optional>
#include <stdexcept>
#include <string>
#include <thread>
#include <vector>

#include <sqlite3.h>

#include "dao/karing_dao.h"
#include "db/db_init.h"
#include "db/db_introspection.h"
#include "db/sqlite_connection.h"
#include "common/error/app_error.h"
#include "domain/entry_operations.h"

namespace fs = std::filesystem;

namespace {

struct test_failure : std::runtime_error {
  using std::runtime_error::runtime_error;
};

struct temp_env {
  fs::path root;
  fs::path db_path;
  fs::path upload_path;
};

void expect(bool condition, const std::string& message) {
  if (!condition) throw test_failure(message);
}

temp_env make_temp_env(const std::string& name) {
  const auto now = std::chrono::steady_clock::now().time_since_epoch().count();
  temp_env env;
  env.root = fs::temp_directory_path() / ("karing-test-" + name + "-" + std::to_string(now));
  env.db_path = env.root / "karing.sqlite";
  env.upload_path = env.root / "uploads";
  fs::create_directories(env.upload_path);
  return env;
}

struct sqlite_db {
  sqlite3* handle{nullptr};
  explicit sqlite_db(const fs::path& path) {
    if (sqlite3_open_v2(path.string().c_str(), &handle, SQLITE_OPEN_READWRITE, nullptr) != SQLITE_OK) {
      throw test_failure("failed to open sqlite db");
    }
  }
  ~sqlite_db() {
    if (handle) sqlite3_close(handle);
  }
};

void exec_sql(sqlite3* db, const std::string& sql) {
  char* errmsg = nullptr;
  const int rc = sqlite3_exec(db, sql.c_str(), nullptr, nullptr, &errmsg);
  if (rc != SQLITE_OK) {
    const std::string message = errmsg ? errmsg : "sqlite exec failed";
    if (errmsg) sqlite3_free(errmsg);
    throw test_failure(message);
  }
}

int query_int(sqlite3* db, const std::string& sql) {
  sqlite3_stmt* stmt = nullptr;
  if (sqlite3_prepare_v2(db, sql.c_str(), -1, &stmt, nullptr) != SQLITE_OK) {
    throw test_failure("failed to prepare int query");
  }
  int value = 0;
  if (sqlite3_step(stmt) == SQLITE_ROW) value = sqlite3_column_int(stmt, 0);
  sqlite3_finalize(stmt);
  return value;
}

std::string query_text(sqlite3* db, const std::string& sql) {
  sqlite3_stmt* stmt = nullptr;
  if (sqlite3_prepare_v2(db, sql.c_str(), -1, &stmt, nullptr) != SQLITE_OK) {
    throw test_failure("failed to prepare text query");
  }
  std::string value;
  if (sqlite3_step(stmt) == SQLITE_ROW) {
    const unsigned char* raw = sqlite3_column_text(stmt, 0);
    value = raw ? reinterpret_cast<const char*>(raw) : "";
  }
  sqlite3_finalize(stmt);
  return value;
}

void test_init_schema_creates_expected_layout() {
  const auto env = make_temp_env("init");
  const auto result = karing::db::init_sqlite_schema_file(env.db_path.string(), 4, false);
  expect(result.ok, "schema init should succeed");
  expect(result.created, "schema init should mark created");
  expect(result.current_max_items == 4, "current_max_items should be 4");

  const auto info = karing::db::inspect::read_health_info(env.db_path.string());
  expect(info.has_value(), "health info should be readable");
  expect(info->max_items == 4, "max_items should be 4");
  expect(info->next_id == 1, "next_id should start at 1");
  expect(info->active_items == 0, "active_items should be 0 after init");

  const auto check = karing::db::inspect::check_schema(env.db_path.string());
  expect(check.ok, "schema check should pass after init");
}

void test_sqlite_connection_uses_wal_and_busy_timeout() {
  const auto env = make_temp_env("connection");
  const auto result = karing::db::init_sqlite_schema_file(env.db_path.string(), 4, false);
  expect(result.ok, "schema init should succeed");

  karing::db::sqlite_connection db(env.db_path.string(), karing::db::sqlite_access::read_write);
  expect(db.ok(), "sqlite connection should open");
  expect(query_text(db.get(), "PRAGMA journal_mode;") == "wal", "journal_mode should be wal");
  expect(query_int(db.get(), "PRAGMA busy_timeout;") == karing::db::kSqliteBusyTimeoutMs,
         "busy_timeout should use project default");
}

void test_sqlite_error_converts_to_common_error() {
  karing::db::sqlite_error error;
  error.kind = karing::db::sqlite_error_kind::busy;
  error.code = SQLITE_BUSY;
  error.extended_code = SQLITE_BUSY;
  error.message = "database is locked";

  const auto app_error = karing::db::to_app_error(error, "SQLite database is busy");
  expect(app_error.category == karing::domain::error_category::unavailable,
         "busy sqlite error should map to unavailable category");
  expect(app_error.code == karing::domain::error_code::sqlite_busy,
         "busy sqlite error should map to sqlite busy code");
  expect(std::string(karing::domain::to_code_string(app_error.code)) == "E_SQLITE_BUSY",
         "sqlite busy code should have stable string");
  expect(app_error.detail.has_value(), "sqlite common error should preserve internal detail");
}

void test_concurrent_writer_waits_for_short_transaction() {
  const auto env = make_temp_env("concurrent-writer");
  const auto result = karing::db::init_sqlite_schema_file(env.db_path.string(), 4, false);
  expect(result.ok, "schema init should succeed");

  karing::db::sqlite_connection lock(env.db_path.string(), karing::db::sqlite_access::read_write);
  expect(lock.ok(), "lock connection should open");
  exec_sql(lock.get(), "BEGIN IMMEDIATE;");

  int inserted_id = -1;
  std::thread writer([&]() {
    karing::dao::KaringDao dao(env.db_path.string(), env.upload_path.string());
    inserted_id = dao.insert_text("waited for lock");
  });

  std::this_thread::sleep_for(std::chrono::milliseconds(100));
  exec_sql(lock.get(), "COMMIT;");
  writer.join();

  expect(inserted_id == 1, "writer should wait for lock and insert slot 1");
}

void test_dao_manages_file_lifecycle() {
  const auto env = make_temp_env("files");
  const auto init = karing::db::init_sqlite_schema_file(env.db_path.string(), 2, false);
  expect(init.ok, "schema init should succeed");

  karing::dao::KaringDao dao(env.db_path.string(), env.upload_path.string());
  const int id = dao.insert_file("first.pdf", "application/pdf", "pdf-a");
  expect(id == 1, "first file insert should use slot 1");

  sqlite_db db(env.db_path);
  const auto first_path = fs::path(query_text(db.handle, "SELECT file_path FROM entries WHERE id=1;"));
  expect(fs::exists(first_path), "first file should exist on disk");

  const bool updated = dao.update_file(1, "second.pdf", "application/pdf", "pdf-b");
  expect(updated, "update_file should succeed");
  expect(!fs::exists(first_path), "old file should be removed after update");

  const auto second_path = fs::path(query_text(db.handle, "SELECT file_path FROM entries WHERE id=1;"));
  expect(fs::exists(second_path), "new file should exist on disk");

  const bool deleted = dao.logical_delete(1);
  expect(deleted, "logical_delete should succeed");
  expect(query_int(db.handle, "SELECT used FROM entries WHERE id=1;") == 0,
         "logical_delete should clear the DB slot");
  expect(query_text(db.handle, "SELECT file_path FROM entries WHERE id=1;").empty(),
         "logical_delete should clear file metadata from the DB slot");
}

void test_text_file_upload_is_text_record_with_blob() {
  const auto env = make_temp_env("text-file");
  const auto init = karing::db::init_sqlite_schema_file(env.db_path.string(), 3, false);
  expect(init.ok, "schema init should succeed");

  karing::dao::KaringDao dao(env.db_path.string(), env.upload_path.string());
  const int id = dao.insert_file("note.txt", "text/plain", "hello text file");
  expect(id == 1, "text file insert should use slot 1");

  const auto record = dao.get_by_id(id);
  expect(record.has_value(), "text file record should be readable");
  expect(!record->is_file, "text file should behave like text record");
  expect(record->filename == "note.txt", "text file should keep filename");
  expect(record->mime == "text/plain", "text file should keep mime");

  std::string mime;
  std::string filename;
  std::string data;
  expect(dao.get_file_blob(id, mime, filename, data), "text file should still expose file blob");
  expect(filename == "note.txt", "blob filename should match");
  expect(mime == "text/plain", "blob mime should match");
  expect(data == "hello text file", "blob content should match");
}

void test_domain_operations_search() {
  const auto env = make_temp_env("domain");
  const auto init = karing::db::init_sqlite_schema_file(env.db_path.string(), 5, false);
  expect(init.ok, "schema init should succeed");

  karing::domain::entry_operations operations(env.db_path.string(), env.upload_path.string(), 5);
  expect(operations.create_text("domain alpha") == 1, "domain operation should create text");
  expect(operations.create_file("domain.txt", "text/plain", "domain file") == 2,
         "domain operation should create text file");

  const auto search = operations.search({
      .q = "domain",
      .limit = 5,
      .type = "text",
      .sort = "id",
      .order = "asc",
  });
  expect(search.error == karing::domain::search_error::none, "domain search should succeed");
  expect(search.records.size() == 2, "domain search should return text-like records");

  expect(search.records.size() == 2, "domain search should keep matching text-like records");
}

void test_force_shrink_reassigns_ids_and_removes_old_files() {
  const auto env = make_temp_env("force-shrink");
  const auto init = karing::db::init_sqlite_schema_file(env.db_path.string(), 5, false);
  expect(init.ok, "schema init should succeed");

  karing::dao::KaringDao dao(env.db_path.string(), env.upload_path.string());
  expect(dao.insert_text("entry-1") == 1, "slot 1 insert");
  expect(dao.insert_file("old.pdf", "application/pdf", "old-pdf") == 2, "slot 2 insert");
  expect(dao.insert_text("entry-3") == 3, "slot 3 insert");
  expect(dao.insert_text("entry-4") == 4, "slot 4 insert");
  expect(dao.insert_text("entry-5") == 5, "slot 5 insert");

  sqlite_db db(env.db_path);
  const auto old_path = fs::path(query_text(db.handle, "SELECT file_path FROM entries WHERE id=2;"));
  expect(fs::exists(old_path), "old file should exist before shrink");

  exec_sql(db.handle,
           "UPDATE entries SET stored_at=10, updated_at=10 WHERE id=1;"
           "UPDATE entries SET stored_at=20, updated_at=20 WHERE id=2;"
           "UPDATE entries SET stored_at=30, updated_at=30 WHERE id=3;"
           "UPDATE entries SET stored_at=40, updated_at=40 WHERE id=4;"
           "UPDATE entries SET stored_at=50, updated_at=50 WHERE id=5;"
           "UPDATE store_state SET next_id=3 WHERE singleton_id=1;");

  const auto shrink_fail = karing::db::init_sqlite_schema_file(env.db_path.string(), 3, false);
  expect(!shrink_fail.ok, "shrink without force should fail");

  const auto shrink_ok = karing::db::init_sqlite_schema_file(env.db_path.string(), 3, true);
  expect(shrink_ok.ok, "force shrink should succeed");
  expect(!fs::exists(old_path), "dropped file should be removed from disk");

  expect(query_int(db.handle, "SELECT max_items FROM store_state WHERE singleton_id=1;") == 3, "max_items should be 3");
  expect(query_int(db.handle, "SELECT next_id FROM store_state WHERE singleton_id=1;") == 1, "next_id should reset to 1");
  expect(query_text(db.handle, "SELECT content_text FROM entries WHERE id=1;") == "entry-3", "oldest kept entry should move to id 1");
  expect(query_text(db.handle, "SELECT content_text FROM entries WHERE id=2;") == "entry-4", "next kept entry should move to id 2");
  expect(query_text(db.handle, "SELECT content_text FROM entries WHERE id=3;") == "entry-5", "newest kept entry should move to id 3");
}

void test_swap_entries_exchanges_slot_contents() {
  const auto env = make_temp_env("swap");
  const auto init = karing::db::init_sqlite_schema_file(env.db_path.string(), 4, false);
  expect(init.ok, "schema init should succeed");

  karing::dao::KaringDao dao(env.db_path.string(), env.upload_path.string());
  expect(dao.insert_text("slot-one") == 1, "slot 1 insert");
  expect(dao.insert_file("two.txt", "text/plain", "slot-two") == 2, "slot 2 insert");

  sqlite_db db(env.db_path);
  const auto first_path_before = query_text(db.handle, "SELECT file_path FROM entries WHERE id=1;");
  const auto second_path_before = query_text(db.handle, "SELECT file_path FROM entries WHERE id=2;");
  expect(first_path_before.empty(), "slot 1 should not have file");
  expect(!second_path_before.empty(), "slot 2 should have file");

  expect(dao.swap_entries(1, 2), "swap_entries should succeed");

  const auto first = dao.get_by_id(1);
  const auto second = dao.get_by_id(2);
  expect(first.has_value(), "slot 1 should still be readable");
  expect(second.has_value(), "slot 2 should still be readable");
  expect(!first->is_file, "text file remains text-like after swap");
  expect(first->filename == "two.txt", "slot 1 should now have slot 2 filename");
  expect(second->content == "slot-one", "slot 2 should now have slot 1 content");

  expect(query_text(db.handle, "SELECT file_path FROM entries WHERE id=1;") == second_path_before,
         "slot 1 should inherit file path");
  expect(query_text(db.handle, "SELECT file_path FROM entries WHERE id=2;").empty(),
         "slot 2 should no longer have a file path");

  std::string mime;
  std::string filename;
  std::string data;
  expect(dao.get_file_blob(1, mime, filename, data), "swapped text file blob should remain readable");
  expect(filename == "two.txt", "swapped blob filename should match");
  expect(data == "slot-two", "swapped blob content should match");
}

void test_move_entry_inserts_before_target_slot() {
  const auto env = make_temp_env("move");
  const auto init = karing::db::init_sqlite_schema_file(env.db_path.string(), 5, false);
  expect(init.ok, "schema init should succeed");

  karing::dao::KaringDao dao(env.db_path.string(), env.upload_path.string());
  expect(dao.insert_text("a") == 1, "slot 1 insert");
  expect(dao.insert_text("b") == 2, "slot 2 insert");
  expect(dao.insert_text("c") == 3, "slot 3 insert");
  expect(dao.insert_text("d") == 4, "slot 4 insert");
  expect(dao.insert_text("e") == 5, "slot 5 insert");

  const auto moved = dao.move_entry_before(5, 2);
  expect(moved.has_value(), "move should succeed");
  expect(moved->first.size() == 5, "move should keep all active records");
  expect(moved->second == 1, "full store should keep next_id wrapped to 1");

  expect(dao.get_by_id(1)->content == "a", "slot 1 should remain a");
  expect(dao.get_by_id(2)->content == "e", "slot 2 should become e");
  expect(dao.get_by_id(3)->content == "b", "slot 3 should become b");
  expect(dao.get_by_id(4)->content == "c", "slot 4 should become c");
  expect(dao.get_by_id(5)->content == "d", "slot 5 should become d");
}

void test_move_entry_preserves_file_blob_mapping() {
  const auto env = make_temp_env("move-file");
  const auto init = karing::db::init_sqlite_schema_file(env.db_path.string(), 5, false);
  expect(init.ok, "schema init should succeed");

  karing::dao::KaringDao dao(env.db_path.string(), env.upload_path.string());
  expect(dao.insert_text("a") == 1, "slot 1 insert");
  expect(dao.insert_text("b") == 2, "slot 2 insert");
  expect(dao.insert_file("e.txt", "text/plain", "file-e") == 3, "slot 3 file insert");

  sqlite_db db(env.db_path);
  const auto file_path_before = query_text(db.handle, "SELECT file_path FROM entries WHERE id=3;");
  expect(!file_path_before.empty(), "file path should exist before move");
  expect(fs::path(file_path_before).filename().string().rfind("entry_3_", 0) != 0,
         "file storage path should not be derived from the source slot id");

  const auto moved = dao.move_entry_before(3, 2);
  expect(moved.has_value(), "move should succeed");

  const auto moved_file = dao.get_by_id(2);
  expect(moved_file.has_value(), "moved file record should be readable");
  expect(moved_file->filename == "e.txt", "moved file record should keep filename");
  expect(query_text(db.handle, "SELECT file_path FROM entries WHERE id=2;") == file_path_before,
         "moved file record should keep stored file path");

  std::string mime;
  std::string filename;
  std::string data;
  expect(dao.get_file_blob(2, mime, filename, data), "moved file blob should remain readable");
  expect(filename == "e.txt", "moved file blob filename should match");
  expect(data == "file-e", "moved file blob content should match");
  expect(dao.get_by_id(3)->content == "b", "record between move range should shift right");
}

void test_move_entry_rolls_back_when_rebuild_fails() {
  const auto env = make_temp_env("move-rollback");
  const auto init = karing::db::init_sqlite_schema_file(env.db_path.string(), 5, false);
  expect(init.ok, "schema init should succeed");

  karing::dao::KaringDao dao(env.db_path.string(), env.upload_path.string());
  expect(dao.insert_text("a") == 1, "slot 1 insert");
  expect(dao.insert_text("b") == 2, "slot 2 insert");
  expect(dao.insert_text("c") == 3, "slot 3 insert");

  sqlite_db db(env.db_path);
  expect(query_int(db.handle, "SELECT next_id FROM store_state WHERE singleton_id=1;") == 4,
         "next_id should be 4 before failed move");
  exec_sql(db.handle, "DROP TABLE entries_fts;");

  const auto moved = dao.move_entry_before(3, 1);
  expect(!moved.has_value(), "move should fail when FTS rebuild cannot run");
  expect(query_text(db.handle, "SELECT content_text FROM entries WHERE id=1;") == "a",
         "slot 1 should remain unchanged after rollback");
  expect(query_text(db.handle, "SELECT content_text FROM entries WHERE id=2;") == "b",
         "slot 2 should remain unchanged after rollback");
  expect(query_text(db.handle, "SELECT content_text FROM entries WHERE id=3;") == "c",
         "slot 3 should remain unchanged after rollback");
  expect(query_int(db.handle, "SELECT next_id FROM store_state WHERE singleton_id=1;") == 4,
         "next_id should remain unchanged after rollback");
}

void test_move_entry_rolls_back_on_unique_constraint() {
  const auto env = make_temp_env("move-constraint");
  const auto init = karing::db::init_sqlite_schema_file(env.db_path.string(), 5, false);
  expect(init.ok, "schema init should succeed");

  karing::dao::KaringDao dao(env.db_path.string(), env.upload_path.string());
  expect(dao.insert_text("a") == 1, "slot 1 insert");
  expect(dao.insert_text("b") == 2, "slot 2 insert");
  expect(dao.insert_text("c") == 3, "slot 3 insert");

  sqlite_db db(env.db_path);
  exec_sql(db.handle, "CREATE UNIQUE INDEX entries_content_unique_test ON entries(content_text) WHERE used=1;");

  const auto moved = dao.move_entry_before(3, 1);
  expect(!moved.has_value(), "move should fail on unique constraint violation");
  expect(query_text(db.handle, "SELECT content_text FROM entries WHERE id=1;") == "a",
         "slot 1 should remain unchanged after constraint rollback");
  expect(query_text(db.handle, "SELECT content_text FROM entries WHERE id=2;") == "b",
         "slot 2 should remain unchanged after constraint rollback");
  expect(query_text(db.handle, "SELECT content_text FROM entries WHERE id=3;") == "c",
         "slot 3 should remain unchanged after constraint rollback");
  expect(query_int(db.handle, "SELECT next_id FROM store_state WHERE singleton_id=1;") == 4,
         "next_id should remain unchanged after constraint rollback");
}

void test_resequence_entries_compacts_ids_from_one() {
  const auto env = make_temp_env("resequence");
  const auto init = karing::db::init_sqlite_schema_file(env.db_path.string(), 5, false);
  expect(init.ok, "schema init should succeed");

  karing::dao::KaringDao dao(env.db_path.string(), env.upload_path.string());
  expect(dao.insert_text("slot-one") == 1, "slot 1 insert");
  expect(dao.insert_text("slot-two") == 2, "slot 2 insert");
  expect(dao.insert_file("slot-three.txt", "text/plain", "slot-three") == 3, "slot 3 insert");
  expect(dao.insert_text("slot-four") == 4, "slot 4 insert");

  sqlite_db db(env.db_path);
  const auto file_path_before = query_text(db.handle, "SELECT file_path FROM entries WHERE id=3;");
  expect(!file_path_before.empty(), "file path should exist before resequence");
  exec_sql(db.handle,
           "UPDATE entries SET stored_at=30, updated_at=30 WHERE id=1;"
           "UPDATE entries SET used=0, source_kind=NULL, media_kind=NULL, content_text=NULL, file_path=NULL, "
           "original_filename=NULL, mime_type=NULL, size_bytes=0, stored_at=NULL, updated_at=NULL WHERE id=2;"
           "UPDATE entries SET stored_at=20, updated_at=20 WHERE id=3;"
           "UPDATE entries SET stored_at=40, updated_at=40 WHERE id=4;"
           "UPDATE store_state SET next_id=5 WHERE singleton_id=1;");

  const auto resequenced = dao.resequence_entries();
  expect(resequenced.has_value(), "resequence should succeed");
  expect(resequenced->first.size() == 3, "resequence should keep three active records");
  expect(resequenced->second == 4, "next_id should become first empty slot");

  const auto first = dao.get_by_id(1);
  expect(first.has_value(), "first resequenced slot should exist");
  expect(first->filename == "slot-three.txt", "oldest active entry should move to id 1");
  expect(query_text(db.handle, "SELECT file_path FROM entries WHERE id=1;") == file_path_before,
         "resequence should keep stored file path with moved file record");
  std::string mime;
  std::string filename;
  std::string data;
  expect(dao.get_file_blob(1, mime, filename, data), "resequence should preserve moved file blob");
  expect(filename == "slot-three.txt", "resequence should preserve moved filename");
  expect(data == "slot-three", "resequence should preserve moved file data");
  expect(query_text(db.handle, "SELECT content_text FROM entries WHERE id=2;") == "slot-one",
         "second active entry should move to id 2");
  expect(query_text(db.handle, "SELECT content_text FROM entries WHERE id=3;") == "slot-four",
         "third active entry should move to id 3");
  expect(query_int(db.handle, "SELECT used FROM entries WHERE id=4;") == 0, "slot 4 should be cleared");
  expect(query_int(db.handle, "SELECT next_id FROM store_state WHERE singleton_id=1;") == 4,
         "next_id should point to first cleared slot");
}

void test_resequence_full_store_resets_next_id_to_one() {
  const auto env = make_temp_env("resequence-full");
  const auto init = karing::db::init_sqlite_schema_file(env.db_path.string(), 3, false);
  expect(init.ok, "schema init should succeed");

  karing::dao::KaringDao dao(env.db_path.string(), env.upload_path.string());
  expect(dao.insert_text("one") == 1, "slot 1 insert");
  expect(dao.insert_text("two") == 2, "slot 2 insert");
  expect(dao.insert_text("three") == 3, "slot 3 insert");

  const auto resequenced = dao.resequence_entries();
  expect(resequenced.has_value(), "full resequence should succeed");
  expect(resequenced->first.size() == 3, "full resequence should keep all records");
  expect(resequenced->second == 1, "full store should wrap next_id to 1");
}

}  // namespace

int main() {
  const std::vector<std::pair<std::string, std::function<void()>>> tests = {
      {"init_schema_creates_expected_layout", test_init_schema_creates_expected_layout},
      {"sqlite_connection_uses_wal_and_busy_timeout", test_sqlite_connection_uses_wal_and_busy_timeout},
      {"sqlite_error_converts_to_common_error", test_sqlite_error_converts_to_common_error},
      {"concurrent_writer_waits_for_short_transaction", test_concurrent_writer_waits_for_short_transaction},
      {"dao_manages_file_lifecycle", test_dao_manages_file_lifecycle},
      {"text_file_upload_is_text_record_with_blob", test_text_file_upload_is_text_record_with_blob},
      {"domain_operations_search", test_domain_operations_search},
      {"force_shrink_reassigns_ids_and_removes_old_files", test_force_shrink_reassigns_ids_and_removes_old_files},
      {"swap_entries_exchanges_slot_contents", test_swap_entries_exchanges_slot_contents},
      {"move_entry_inserts_before_target_slot", test_move_entry_inserts_before_target_slot},
      {"move_entry_preserves_file_blob_mapping", test_move_entry_preserves_file_blob_mapping},
      {"move_entry_rolls_back_when_rebuild_fails", test_move_entry_rolls_back_when_rebuild_fails},
      {"move_entry_rolls_back_on_unique_constraint", test_move_entry_rolls_back_on_unique_constraint},
      {"resequence_entries_compacts_ids_from_one", test_resequence_entries_compacts_ids_from_one},
      {"resequence_full_store_resets_next_id_to_one", test_resequence_full_store_resets_next_id_to_one},
  };

  int failed = 0;
  for (const auto& [name, test] : tests) {
    try {
      test();
      std::cout << "[PASS] " << name << "\n";
    } catch (const std::exception& ex) {
      ++failed;
      std::cerr << "[FAIL] " << name << ": " << ex.what() << "\n";
    }
  }

  return failed == 0 ? 0 : 1;
}

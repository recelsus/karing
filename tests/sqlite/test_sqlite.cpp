#include <chrono>
#include <filesystem>
#include <fstream>
#include <functional>
#include <iostream>
#include <optional>
#include <stdexcept>
#include <string>
#include <vector>

#include <sqlite3.h>

#include "dao/karing_dao.h"
#include "db/db_init.h"
#include "db/db_introspection.h"

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
  expect(!fs::exists(second_path), "file should be removed after delete");
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

}  // namespace

int main() {
  const std::vector<std::pair<std::string, std::function<void()>>> tests = {
      {"init_schema_creates_expected_layout", test_init_schema_creates_expected_layout},
      {"dao_manages_file_lifecycle", test_dao_manages_file_lifecycle},
      {"text_file_upload_is_text_record_with_blob", test_text_file_upload_is_text_record_with_blob},
      {"force_shrink_reassigns_ids_and_removes_old_files", test_force_shrink_reassigns_ids_and_removes_old_files},
      {"swap_entries_exchanges_slot_contents", test_swap_entries_exchanges_slot_contents},
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

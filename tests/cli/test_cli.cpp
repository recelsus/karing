#include <chrono>
#include <cstdio>
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <functional>
#include <iostream>
#include <stdexcept>
#include <string>
#include <vector>

#include <sqlite3.h>
#include <fcntl.h>
#include <unistd.h>

#include "app.h"
#include "backend/target.h"
#include "backend/sqlite_backend.h"
#include "dao/karing_dao.h"
#include "db/db_init.h"
#include "db/db_introspection.h"
#include "common/error/app_error.h"
#include "utils/arg_utils.h"
#include "utils/mime.h"

namespace fs = std::filesystem;

namespace {

struct test_failure : std::runtime_error {
  using std::runtime_error::runtime_error;
};

void expect(bool condition, const std::string& message) {
  if (!condition) throw test_failure(message);
}

void exec_sql(sqlite3* db, const std::string& sql) {
  char* errmsg = nullptr;
  const int rc = sqlite3_exec(db, sql.c_str(), nullptr, nullptr, &errmsg);
  if (rc != SQLITE_OK) {
    const std::string message = errmsg ? errmsg : "sqlite exec failed";
    if (errmsg) sqlite3_free(errmsg);
    throw test_failure(message);
  }
}

fs::path make_temp_root(const std::string& name) {
  const auto now = std::chrono::steady_clock::now().time_since_epoch().count();
  const auto root = fs::temp_directory_path() / ("karing-cli-test-" + name + "-" + std::to_string(now));
  fs::create_directories(root);
  return root;
}

void write_file(const fs::path& path, const std::string& data) {
  std::ofstream out(path, std::ios::binary | std::ios::trunc);
  if (!out.is_open()) throw test_failure("failed to create fixture");
  out.write(data.data(), static_cast<std::streamsize>(data.size()));
}

std::string capture_stderr(const fs::path& path, const std::function<int()>& fn, int& status) {
  const int saved = dup(STDERR_FILENO);
  if (saved < 0) throw test_failure("failed to duplicate stderr");
  const int out = open(path.string().c_str(), O_CREAT | O_TRUNC | O_WRONLY, 0600);
  if (out < 0) {
    close(saved);
    throw test_failure("failed to open stderr capture");
  }
  if (dup2(out, STDERR_FILENO) < 0) {
    close(out);
    close(saved);
    throw test_failure("failed to redirect stderr");
  }
  close(out);

  status = fn();
  std::fflush(stderr);
  if (dup2(saved, STDERR_FILENO) < 0) {
    close(saved);
    throw test_failure("failed to restore stderr");
  }
  close(saved);

  std::ifstream in(path, std::ios::binary);
  return std::string(std::istreambuf_iterator<char>(in), std::istreambuf_iterator<char>());
}

void test_arg_utils() {
  expect(karing::cli::utils::join_words({"hello", "world"}) == "hello world", "join_words should join with spaces");
  expect(karing::cli::utils::is_valid_id("1"), "id 1 should be valid");
  expect(karing::cli::utils::is_valid_id("1000"), "id 1000 should be valid");
  expect(!karing::cli::utils::is_valid_id("0"), "id 0 should be invalid");
  expect(!karing::cli::utils::is_valid_id("1001"), "id 1001 should be invalid");
  expect(!karing::cli::utils::is_valid_id("text"), "non-numeric id should be invalid");
}

void test_target_parsing() {
  const auto http = karing::cli::backend::parse_target("http://127.0.0.1:8080/");
  expect(http.kind == karing::cli::backend::target_kind::http, "http URL should select HTTP backend");
  expect(http.value == "http://127.0.0.1:8080", "http target should be normalized");

  const auto https = karing::cli::backend::parse_target("https://example.test/karing");
  expect(https.kind == karing::cli::backend::target_kind::http, "https URL should select HTTP backend");

  const auto sqlite = karing::cli::backend::parse_target("./karing.sqlite");
  expect(sqlite.kind == karing::cli::backend::target_kind::sqlite, "path should select SQLite backend");
  expect(sqlite.value == "./karing.sqlite", "sqlite target should preserve path");
}

void test_mime_guessing() {
  const auto root = make_temp_root("mime");
  const auto markdown = root / "note.md";
  const auto source = root / "main.cpp";
  const auto extensionless_text = root / ".bashrc";
  const auto binary = root / "blob.unknown";

  write_file(markdown, "# note\n");
  write_file(source, "int main() { return 0; }\n");
  write_file(extensionless_text, "alias ll='ls -la'\n");
  write_file(binary, std::string("abc\0def", 7));

  expect(karing::cli::utils::guess_mime_type(markdown).value_or("") == "text/markdown", "markdown mime should be inferred");
  expect(karing::cli::utils::guess_mime_type(source).value_or("") == "text/plain", "source mime should be inferred as text/plain");
  expect(karing::cli::utils::guess_mime_type(extensionless_text).value_or("") == "text/plain", "text-like extensionless files should be text/plain");
  expect(!karing::cli::utils::guess_mime_type(binary).has_value(), "unknown binary files should not get a text mime");
}

void test_sqlite_backend_text_workflow() {
  const auto root = make_temp_root("sqlite-backend");
  const auto db_path = root / "karing.sqlite";
  const auto upload_path = root / "uploads";
  const auto init = karing::db::init_sqlite_schema_file(db_path.string(), 5, false);
  expect(init.ok, "schema init should succeed");

  const karing::cli::backend::sqlite_context context{
      .db_path = db_path.string(),
      .json_output = false,
  };

  expect(karing::cli::backend::run_sqlite_add(context, {"alpha", "note"}) == 0,
         "sqlite backend should add text");
  expect(karing::cli::backend::run_sqlite_add(context, {"beta", "note"}) == 0,
         "sqlite backend should add second text");
  expect(karing::cli::backend::run_sqlite_get(context, 1) == 0,
         "sqlite backend should get text by id");
  expect(karing::cli::backend::run_sqlite_find(context, {"note", "--sort", "id", "--asc"}) == 0,
         "sqlite backend should search text");
  expect(karing::cli::backend::run_sqlite_mod(context, {"1", "alpha", "changed"}) == 0,
         "sqlite backend should update text");
  expect(karing::cli::backend::run_sqlite_move(context, {"2", "1"}) == 0,
         "sqlite backend should move a record before another record");
  expect(karing::cli::backend::run_sqlite_swap(context, {"1", "2"}) == 0,
         "sqlite backend should swap records");
  expect(karing::cli::backend::run_sqlite_resequence(context) == 0,
         "sqlite backend should resequence records");
  expect(karing::cli::backend::run_sqlite_add(context, {"-f", "README.md"}) != 0,
         "sqlite backend should reject file upload");

  karing::dao::KaringDao dao(db_path.string());
  const auto first = dao.get_by_id(1);
  const auto second = dao.get_by_id(2);
  expect(first.has_value(), "first record should exist");
  expect(second.has_value(), "second record should exist");
  expect(first->content == "alpha changed", "move, swap, and resequence should preserve first content");
  expect(second->content == "beta note", "move, update, and resequence should preserve second content");

  expect(karing::cli::backend::run_sqlite_delete(context, {"1"}) == 0,
         "sqlite backend should delete text record");
  expect(!dao.get_by_id(1).has_value(), "deleted record should no longer be active");
}

void test_sqlite_add_reuses_recently_deleted_slot() {
  const auto root = make_temp_root("slot-reuse");
  const auto db_path = root / "karing.sqlite";
  const auto init = karing::db::init_sqlite_schema_file(db_path.string(), 3, false);
  expect(init.ok, "schema init should succeed");

  const karing::cli::backend::sqlite_context context{
      .db_path = db_path.string(),
      .json_output = false,
  };
  expect(karing::cli::backend::run_sqlite_add(context, {"first"}) == 0, "first add should succeed");
  expect(karing::cli::backend::run_sqlite_add(context, {"second"}) == 0, "second add should succeed");
  expect(karing::cli::backend::run_sqlite_delete(context, {}) == 0, "recent latest delete should succeed");
  expect(karing::cli::backend::run_sqlite_add(context, {"replacement"}) == 0, "add after delete should succeed");

  karing::dao::KaringDao dao(db_path.string());
  const auto reused = dao.get_by_id(2);
  expect(reused.has_value(), "deleted slot should be reused");
  expect(reused->content == "replacement", "reused slot should contain new content");
}

void test_sqlite_delete_without_id_rejects_expired_latest() {
  const auto root = make_temp_root("expired-delete");
  const auto db_path = root / "karing.sqlite";
  const auto init = karing::db::init_sqlite_schema_file(db_path.string(), 3, false);
  expect(init.ok, "schema init should succeed");

  const karing::cli::backend::sqlite_context context{
      .db_path = db_path.string(),
      .json_output = false,
  };
  expect(karing::cli::backend::run_sqlite_add(context, {"expired"}) == 0, "add should succeed");

  sqlite3* db = nullptr;
  expect(sqlite3_open_v2(db_path.string().c_str(), &db, SQLITE_OPEN_READWRITE, nullptr) == SQLITE_OK,
         "sqlite db should open");
  exec_sql(db, "UPDATE entries SET stored_at=strftime('%s','now') - 601 WHERE id=1;");
  sqlite3_close(db);

  expect(karing::cli::backend::run_sqlite_delete(context, {}) != 0,
         "delete without id should reject expired latest record");
  karing::dao::KaringDao dao(db_path.string());
  expect(dao.get_by_id(1).has_value(), "expired latest record should remain active");
}

void test_sqlite_init_database_requires_explicit_path_and_protects_existing_file() {
  const auto root = make_temp_root("sqlite-init");
  const auto db_path = root / "created.sqlite";

  expect(karing::cli::backend::run_sqlite_init_database({}, false) != 0,
         "init-db should require an explicit path");
  expect(karing::cli::backend::run_sqlite_init_database({db_path.string(), "--limit", "4"}, false) == 0,
         "init-db should create sqlite database");
  expect(fs::exists(db_path), "init-db should create db file");

  const auto check = karing::db::inspect::check_schema(db_path.string());
  expect(check.ok, "created sqlite database should pass schema check");

  expect(karing::cli::backend::run_sqlite_init_database({db_path.string()}, false) != 0,
         "init-db should protect existing db without force");
  expect(karing::cli::backend::run_sqlite_init_database({db_path.string(), "--force", "--limit", "3"}, false) == 0,
         "init-db force should allow explicit reinitialize or resize");
}

void test_common_cli_error_codes_are_stable() {
  expect(std::string(karing::domain::to_code_string(karing::domain::error_code::validation)) == "E_VALIDATION",
         "validation code string should remain compatible");
  expect(std::string(karing::domain::to_code_string(karing::domain::error_code::backend_unsupported)) == "E_BACKEND",
         "backend unsupported code string should be stable");
  expect(std::string(karing::domain::to_code_string(karing::domain::error_code::fts_unavailable)) == "E_FTS_UNAVAILABLE",
         "fts unavailable code string should remain compatible");

  const auto root = make_temp_root("sqlite-json-error");
  const auto db_path = root / "common-error.sqlite";
  const auto init = karing::db::init_sqlite_schema_file(db_path.string(), 3, false);
  expect(init.ok, "sqlite common error test db should initialize");
  const karing::cli::backend::sqlite_context context{
      .db_path = db_path.string(),
      .json_output = true,
  };
  expect(karing::cli::backend::run_sqlite_add(context, {"-f", "README.md"}) != 0,
         "sqlite backend JSON mode should reject file upload through common error path");
}

void test_sqlite_busy_errors_use_common_cli_output() {
  const auto root = make_temp_root("sqlite-busy");
  const auto db_path = root / "busy.sqlite";
  const auto init = karing::db::init_sqlite_schema_file(db_path.string(), 3, false);
  expect(init.ok, "busy test db should initialize");

  sqlite3* db = nullptr;
  expect(sqlite3_open_v2(db_path.string().c_str(), &db, SQLITE_OPEN_READWRITE, nullptr) == SQLITE_OK,
         "busy lock db should open");
  exec_sql(db, "BEGIN IMMEDIATE;");

  int plain_status = 0;
  const karing::cli::backend::sqlite_context plain_context{
      .db_path = db_path.string(),
      .json_output = false,
  };
  const auto plain_error = capture_stderr(root / "plain.err", [&]() {
    return karing::cli::backend::run_sqlite_add(plain_context, {"busy"});
  }, plain_status);
  expect(plain_status != 0, "plain busy add should fail");
  expect(plain_error.find("ERROR: database is busy") != std::string::npos,
         "plain busy error should be concise");

  int json_status = 0;
  const karing::cli::backend::sqlite_context json_context{
      .db_path = db_path.string(),
      .json_output = true,
  };
  const auto json_error = capture_stderr(root / "json.err", [&]() {
    return karing::cli::backend::run_sqlite_add(json_context, {"busy"});
  }, json_status);
  expect(json_status != 0, "json busy add should fail");
  expect(json_error.find("\"code\":\"E_SQLITE_BUSY\"") != std::string::npos,
         "json busy error should expose sqlite busy code");

  exec_sql(db, "ROLLBACK;");
  sqlite3_close(db);
}

void test_cli_routes_explicit_commands_without_implicit_add() {
  const auto root = make_temp_root("cli-routing");
  const auto db_path = root / "routing.sqlite";
  const auto init = karing::db::init_sqlite_schema_file(db_path.string(), 3, false);
  expect(init.ok, "routing db should initialize");

  {
    setenv("KARING_TARGET", db_path.string().c_str(), 1);
    char arg0[] = "karing";
    char arg1[] = "add";
    char arg2[] = "routed";
    char* argv[] = {arg0, arg1, arg2};
    expect(karing::cli::run(3, argv) == 0, "explicit add command should add text");
  }

  {
    char arg0[] = "karing";
    char arg1[] = "--target";
    std::string target = db_path.string();
    char arg3[] = "get";
    char arg4[] = "1";
    char* argv[] = {arg0, arg1, target.data(), arg3, arg4};
    expect(karing::cli::run(5, argv) == 0, "explicit get command should fetch text");
  }

  {
    char arg0[] = "karing";
    char arg1[] = "--target";
    std::string target = db_path.string();
    char arg3[] = "40";
    char* argv[] = {arg0, arg1, target.data(), arg3};
    expect(karing::cli::run(4, argv) != 0, "numeric shorthand should not add text when record is missing");
  }

  {
    char arg0[] = "karing";
    char arg1[] = "--target";
    std::string target = db_path.string();
    char arg3[] = "1001";
    char* argv[] = {arg0, arg1, target.data(), arg3};
    expect(karing::cli::run(4, argv) != 0, "out-of-range numeric shorthand should be not found");
  }

  {
    char arg0[] = "karing";
    char arg1[] = "--target";
    std::string target = db_path.string();
    char arg3[] = "anything";
    char* argv[] = {arg0, arg1, target.data(), arg3};
    expect(karing::cli::run(4, argv) != 0, "unknown positional string should print help instead of adding");
  }

  {
    char arg0[] = "karing";
    char arg1[] = "del";
    char* argv[] = {arg0, arg1};
    expect(karing::cli::run(2, argv) == 0, "del without id should delete recent latest record");
  }

  karing::dao::KaringDao dao(db_path.string());
  expect(!dao.get_by_id(1).has_value(), "recent delete should remove the created record");
  expect(!dao.get_by_id(2).has_value(), "unknown positional string should not create a second record");
  unsetenv("KARING_TARGET");

  {
    char arg0[] = "karing";
    char* argv[] = {arg0};
    expect(karing::cli::run(1, argv) != 0, "missing target should fail when env and --target are absent");
  }
}

}  // namespace

int main() {
  const std::vector<std::pair<std::string, std::function<void()>>> tests = {
      {"arg_utils", test_arg_utils},
      {"target_parsing", test_target_parsing},
      {"mime_guessing", test_mime_guessing},
      {"sqlite_backend_text_workflow", test_sqlite_backend_text_workflow},
      {"sqlite_add_reuses_recently_deleted_slot", test_sqlite_add_reuses_recently_deleted_slot},
      {"sqlite_delete_without_id_rejects_expired_latest", test_sqlite_delete_without_id_rejects_expired_latest},
      {"sqlite_init_database_requires_explicit_path_and_protects_existing_file", test_sqlite_init_database_requires_explicit_path_and_protects_existing_file},
      {"common_cli_error_codes_are_stable", test_common_cli_error_codes_are_stable},
      {"sqlite_busy_errors_use_common_cli_output", test_sqlite_busy_errors_use_common_cli_output},
      {"cli_routes_explicit_commands_without_implicit_add", test_cli_routes_explicit_commands_without_implicit_add},
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

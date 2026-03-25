// SQLite schema init and resize using raw C API.
#include "db_init_internal.h"

namespace karing::db {

init_result init_sqlite_schema_file(const std::string& db_path_str, int max_items, bool force) {
  init_result result;
  result.current_max_items = max_items;

  sqlite3* db = nullptr;
  if (sqlite3_open_v2(db_path_str.c_str(), &db, SQLITE_OPEN_READWRITE | SQLITE_OPEN_CREATE, nullptr) != SQLITE_OK) {
    result.error = db ? sqlite3_errmsg(db) : "sqlite open failed";
    if (db) sqlite3_close(db);
    return result;
  }

  const auto finish = [&](bool ok) {
    if (db) sqlite3_close(db);
    result.ok = ok;
    return result;
  };

  std::string error;
  std::string res;
  auto cb = [](void* p, int argc, char** argv, char**) -> int {
    if (argc > 0 && argv && argv[0]) *static_cast<std::string*>(p) = argv[0];
    return 0;
  };
  char* err = nullptr;
  sqlite3_exec(db, "PRAGMA integrity_check;", cb, &res, &err);
  if (err) sqlite3_free(err);
  if (!res.empty() && res != "ok") {
    result.error = "SQLite integrity_check failed: " + res;
    return finish(false);
  }

  if (!detail::exec_stmt(db, "PRAGMA journal_mode = DELETE;", error) ||
      !detail::exec_stmt(db, "PRAGMA synchronous = NORMAL;", error) ||
      !detail::exec_stmt(db, "PRAGMA foreign_keys = ON;", error) ||
      !detail::exec_stmt(db, "BEGIN IMMEDIATE;", error)) {
    result.error = error;
    detail::exec_stmt(db, "ROLLBACK;", error);
    return finish(false);
  }

  const bool entries_exists = detail::has_table(db, "entries", error);
  if (!error.empty()) {
    result.error = error;
    detail::exec_stmt(db, "ROLLBACK;", error);
    return finish(false);
  }
  result.created = !entries_exists;

  if (!detail::prepare_schema(db, max_items, result, error)) {
    result.error = error;
    detail::exec_stmt(db, "ROLLBACK;", error);
    return finish(false);
  }

  bool reset_next_id = false;
  std::vector<std::string> files_to_remove;
  if (!detail::apply_resize(db, max_items, force, result, files_to_remove, reset_next_id, error)) {
    result.error = error;
    detail::exec_stmt(db, "ROLLBACK;", error);
    return finish(false);
  }

  if (!detail::finalize_schema(db, result.current_max_items, reset_next_id, error) ||
      !detail::exec_stmt(db, "COMMIT;", error)) {
    result.error = error;
    detail::exec_stmt(db, "ROLLBACK;", error);
    return finish(false);
  }

  detail::remove_files(files_to_remove);
  return finish(true);
}

}  // namespace karing::db

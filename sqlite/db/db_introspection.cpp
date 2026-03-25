#include "db_introspection.h"

#include <sqlite3.h>

namespace karing::db::inspect {

namespace {

bool table_exists(sqlite3* db, const char* name, std::string& error) {
  sqlite3_stmt* stmt = nullptr;
  if (sqlite3_prepare_v2(db, "SELECT 1 FROM sqlite_master WHERE type='table' AND name=? LIMIT 1;", -1, &stmt, nullptr) != SQLITE_OK) {
    error = sqlite3_errmsg(db);
    return false;
  }
  sqlite3_bind_text(stmt, 1, name, -1, SQLITE_TRANSIENT);
  const bool exists = sqlite3_step(stmt) == SQLITE_ROW;
  sqlite3_finalize(stmt);
  return exists;
}

bool metadata_value(sqlite3* db, const char* key, std::string& value, std::string& error) {
  sqlite3_stmt* stmt = nullptr;
  if (sqlite3_prepare_v2(db, "SELECT value_text FROM metadata WHERE key=?;", -1, &stmt, nullptr) != SQLITE_OK) {
    error = sqlite3_errmsg(db);
    return false;
  }
  sqlite3_bind_text(stmt, 1, key, -1, SQLITE_TRANSIENT);
  const bool ok = sqlite3_step(stmt) == SQLITE_ROW;
  if (ok) {
    const unsigned char* raw = sqlite3_column_text(stmt, 0);
    value = raw ? reinterpret_cast<const char*>(raw) : "";
  }
  sqlite3_finalize(stmt);
  return ok;
}

}  // namespace

std::vector<std::pair<std::string, std::string>> list_tables_with_sql(const std::string& db_path) {
  sqlite3* db = nullptr;
  std::vector<std::pair<std::string, std::string>> out;
  if (sqlite3_open_v2(db_path.c_str(), &db, SQLITE_OPEN_READONLY, nullptr) != SQLITE_OK) {
    if (db) sqlite3_close(db);
    return out;
  }
  const char* sql = "SELECT name, sql FROM sqlite_master WHERE type='table' ORDER BY name;";
  sqlite3_stmt* stmt = nullptr;
  if (sqlite3_prepare_v2(db, sql, -1, &stmt, nullptr) == SQLITE_OK) {
    while (sqlite3_step(stmt) == SQLITE_ROW) {
      const unsigned char* n = sqlite3_column_text(stmt, 0);
      const unsigned char* s = sqlite3_column_text(stmt, 1);
      out.emplace_back(n ? reinterpret_cast<const char*>(n) : "",
                       s ? reinterpret_cast<const char*>(s) : "");
    }
  }
  if (stmt) sqlite3_finalize(stmt);
  sqlite3_close(db);
  return out;
}

schema_check_result check_schema(const std::string& db_path) {
  sqlite3* db = nullptr;
  schema_check_result result;
  if (sqlite3_open_v2(db_path.c_str(), &db, SQLITE_OPEN_READONLY, nullptr) != SQLITE_OK) {
    result.error = db ? sqlite3_errmsg(db) : "sqlite open failed";
    if (db) sqlite3_close(db);
    return result;
  }

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
    sqlite3_close(db);
    return result;
  }

  for (const char* table : {"metadata", "store_state", "entries", "entries_fts"}) {
    if (!table_exists(db, table, error)) {
      result.error = error.empty() ? std::string("missing table: ") + table : error;
      sqlite3_close(db);
      return result;
    }
  }

  sqlite3_stmt* stmt = nullptr;
  if (sqlite3_prepare_v2(db, "SELECT max_items, next_id FROM store_state WHERE singleton_id=1;", -1, &stmt, nullptr) != SQLITE_OK) {
    result.error = sqlite3_errmsg(db);
    sqlite3_close(db);
    return result;
  }
  const bool has_store_state = sqlite3_step(stmt) == SQLITE_ROW;
  sqlite3_finalize(stmt);
  if (!has_store_state) {
    result.error = "store_state is missing singleton row";
    sqlite3_close(db);
    return result;
  }

  std::string schema_name;
  if (!metadata_value(db, "schema_name", schema_name, error)) {
    result.error = error.empty() ? "metadata.schema_name is missing" : error;
    sqlite3_close(db);
    return result;
  }
  if (schema_name != "karing_v2") {
    result.error = "unexpected schema_name: " + schema_name;
    sqlite3_close(db);
    return result;
  }

  result.ok = true;
  sqlite3_close(db);
  return result;
}

std::optional<health_info> read_health_info(const std::string& db_path) {
  sqlite3* db = nullptr;
  if (sqlite3_open_v2(db_path.c_str(), &db, SQLITE_OPEN_READONLY, nullptr) != SQLITE_OK) {
    if (db) sqlite3_close(db);
    return std::nullopt;
  }

  health_info out;

  sqlite3_stmt* stmt = nullptr;
  if (sqlite3_prepare_v2(db,
                         "SELECT max_items, next_id FROM store_state WHERE singleton_id=1;",
                         -1,
                         &stmt,
                         nullptr) != SQLITE_OK) {
    sqlite3_close(db);
    return std::nullopt;
  }
  if (sqlite3_step(stmt) == SQLITE_ROW) {
    out.max_items = sqlite3_column_int(stmt, 0);
    out.next_id = sqlite3_column_int(stmt, 1);
  }
  sqlite3_finalize(stmt);

  if (sqlite3_prepare_v2(db, "SELECT COUNT(1) FROM entries WHERE used=1;", -1, &stmt, nullptr) != SQLITE_OK) {
    sqlite3_close(db);
    return std::nullopt;
  }
  if (sqlite3_step(stmt) == SQLITE_ROW) {
    out.active_items = sqlite3_column_int(stmt, 0);
  }
  sqlite3_finalize(stmt);

  sqlite3_close(db);
  return out;
}

}

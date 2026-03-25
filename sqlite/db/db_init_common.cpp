#include "db_init_internal.h"

#include "schema_sql.h"

namespace karing::db::detail {

bool exec_sql(sqlite3* db, const std::string& sql, std::string& error) {
  char* errmsg = nullptr;
  const int rc = sqlite3_exec(db, sql.c_str(), nullptr, nullptr, &errmsg);
  if (rc != SQLITE_OK) {
    error = errmsg ? errmsg : sqlite3_errmsg(db);
    if (errmsg) sqlite3_free(errmsg);
    return false;
  }
  return true;
}

bool exec_stmt(sqlite3* db, const char* sql, std::string& error) {
  return exec_sql(db, sql, error);
}

bool has_table(sqlite3* db, const char* table_name, std::string& error) {
  sqlite3_stmt* stmt = nullptr;
  if (sqlite3_prepare_v2(db, "SELECT 1 FROM sqlite_master WHERE type='table' AND name=? LIMIT 1;", -1, &stmt, nullptr) != SQLITE_OK) {
    error = sqlite3_errmsg(db);
    return false;
  }
  sqlite3_bind_text(stmt, 1, table_name, -1, SQLITE_TRANSIENT);
  const bool exists = (sqlite3_step(stmt) == SQLITE_ROW);
  sqlite3_finalize(stmt);
  return exists;
}

bool seed_metadata(sqlite3* db, std::string& error) {
  sqlite3_stmt* stmt = nullptr;
  const char* sql =
      "INSERT INTO metadata(key, value_text, updated_at) VALUES(?, ?, strftime('%s','now')) "
      "ON CONFLICT(key) DO UPDATE SET value_text=excluded.value_text, updated_at=excluded.updated_at;";
  if (sqlite3_prepare_v2(db, sql, -1, &stmt, nullptr) != SQLITE_OK) {
    error = sqlite3_errmsg(db);
    return false;
  }

  const auto bind_and_step = [&](const char* key, const std::string& value) -> bool {
    sqlite3_reset(stmt);
    sqlite3_clear_bindings(stmt);
    sqlite3_bind_text(stmt, 1, key, -1, SQLITE_TRANSIENT);
    sqlite3_bind_text(stmt, 2, value.c_str(), -1, SQLITE_TRANSIENT);
    return sqlite3_step(stmt) == SQLITE_DONE;
  };

  const bool ok = bind_and_step("schema_version", std::to_string(kSchemaVersion)) &&
                  bind_and_step("schema_name", "karing_v2") &&
                  bind_and_step("fts_table", "entries_fts");
  if (!ok) error = sqlite3_errmsg(db);
  sqlite3_finalize(stmt);
  return ok;
}

bool ensure_store_state(sqlite3* db, int max_items, bool& created, int& previous_max_items, std::string& error) {
  sqlite3_stmt* stmt = nullptr;
  if (sqlite3_prepare_v2(db, "SELECT max_items FROM store_state WHERE singleton_id=1;", -1, &stmt, nullptr) != SQLITE_OK) {
    error = sqlite3_errmsg(db);
    return false;
  }

  bool exists = false;
  if (sqlite3_step(stmt) == SQLITE_ROW) {
    previous_max_items = sqlite3_column_int(stmt, 0);
    exists = true;
  }
  sqlite3_finalize(stmt);

  if (exists) return true;

  created = true;
  previous_max_items = 0;
  if (sqlite3_prepare_v2(db,
                         "INSERT INTO store_state(singleton_id, max_items, next_id, updated_at) "
                         "VALUES(1, ?, 1, strftime('%s','now'));",
                         -1,
                         &stmt,
                         nullptr) != SQLITE_OK) {
    error = sqlite3_errmsg(db);
    return false;
  }
  sqlite3_bind_int(stmt, 1, max_items);
  const bool ok = sqlite3_step(stmt) == SQLITE_DONE;
  if (!ok) error = sqlite3_errmsg(db);
  sqlite3_finalize(stmt);
  return ok;
}

bool ensure_slots(sqlite3* db, int start_id, int end_id, std::string& error) {
  if (start_id > end_id) return true;
  sqlite3_stmt* stmt = nullptr;
  if (sqlite3_prepare_v2(db,
                         "INSERT OR IGNORE INTO entries("
                         "id, used, source_kind, media_kind, content_text, file_path, original_filename, mime_type, size_bytes, stored_at, updated_at"
                         ") VALUES(?, 0, NULL, NULL, NULL, NULL, NULL, NULL, 0, NULL, NULL);",
                         -1,
                         &stmt,
                         nullptr) != SQLITE_OK) {
    error = sqlite3_errmsg(db);
    return false;
  }

  for (int id = start_id; id <= end_id; ++id) {
    sqlite3_reset(stmt);
    sqlite3_clear_bindings(stmt);
    sqlite3_bind_int(stmt, 1, id);
    if (sqlite3_step(stmt) != SQLITE_DONE) {
      error = sqlite3_errmsg(db);
      sqlite3_finalize(stmt);
      return false;
    }
  }

  sqlite3_finalize(stmt);
  return true;
}

bool update_store_state(sqlite3* db, int max_items, bool reset_next_id, std::string& error) {
  sqlite3_stmt* stmt = nullptr;
  const char* sql = reset_next_id
      ? "UPDATE store_state SET max_items=?, next_id=1, updated_at=strftime('%s','now') WHERE singleton_id=1;"
      : "UPDATE store_state SET max_items=?, next_id=CASE WHEN next_id > ? THEN 1 ELSE next_id END, updated_at=strftime('%s','now') WHERE singleton_id=1;";
  if (sqlite3_prepare_v2(db, sql, -1, &stmt, nullptr) != SQLITE_OK) {
    error = sqlite3_errmsg(db);
    return false;
  }
  sqlite3_bind_int(stmt, 1, max_items);
  if (!reset_next_id) sqlite3_bind_int(stmt, 2, max_items);
  const bool ok = sqlite3_step(stmt) == SQLITE_DONE;
  if (!ok) error = sqlite3_errmsg(db);
  sqlite3_finalize(stmt);
  return ok;
}

bool drop_fts_objects(sqlite3* db, std::string& error) {
  return exec_stmt(db, "DROP TRIGGER IF EXISTS entries_ai;", error) &&
         exec_stmt(db, "DROP TRIGGER IF EXISTS entries_au;", error) &&
         exec_stmt(db, "DROP TRIGGER IF EXISTS entries_ad;", error) &&
         exec_stmt(db, "DROP TABLE IF EXISTS entries_fts;", error);
}

bool rebuild_fts(sqlite3* db, std::string& error) {
  return exec_sql(db, schema_sql::kSchemaFtsSql, error) &&
         exec_stmt(db, "INSERT INTO entries_fts(entries_fts) VALUES('rebuild');", error);
}

bool prepare_schema(sqlite3* db, int max_items, init_result& result, std::string& error) {
  if (!exec_sql(db, schema_sql::kSchemaBaseSql, error)) return false;

  bool created_state = false;
  if (!ensure_store_state(db, max_items, created_state, result.previous_max_items, error)) return false;

  const int current_max_items = result.previous_max_items == 0 ? max_items : result.previous_max_items;
  return ensure_slots(db, 1, current_max_items, error);
}

bool finalize_schema(sqlite3* db, int current_max_items, bool reset_next_id, std::string& error) {
  return update_store_state(db, current_max_items, reset_next_id, error) &&
         drop_fts_objects(db, error) &&
         rebuild_fts(db, error) &&
         seed_metadata(db, error);
}

}  // namespace karing::db::detail

#include "db_init_internal.h"

#include <filesystem>

namespace karing::db::detail {

std::string column_text(sqlite3_stmt* stmt, int index) {
  const unsigned char* value = sqlite3_column_text(stmt, index);
  return value ? reinterpret_cast<const char*>(value) : std::string();
}

bool load_active_entries(sqlite3* db, std::vector<active_entry>& entries, std::string& error) {
  sqlite3_stmt* stmt = nullptr;
  const char* sql =
      "SELECT source_kind, media_kind, content_text, file_path, original_filename, mime_type, "
      "size_bytes, stored_at, updated_at "
      "FROM entries WHERE used = 1 ORDER BY stored_at ASC, id ASC;";
  if (sqlite3_prepare_v2(db, sql, -1, &stmt, nullptr) != SQLITE_OK) {
    error = sqlite3_errmsg(db);
    return false;
  }

  int rc = SQLITE_ROW;
  while ((rc = sqlite3_step(stmt)) == SQLITE_ROW) {
    active_entry entry;
    entry.has_source_kind = sqlite3_column_type(stmt, 0) != SQLITE_NULL;
    entry.source_kind = column_text(stmt, 0);
    entry.has_media_kind = sqlite3_column_type(stmt, 1) != SQLITE_NULL;
    entry.media_kind = column_text(stmt, 1);
    entry.has_content_text = sqlite3_column_type(stmt, 2) != SQLITE_NULL;
    entry.content_text = column_text(stmt, 2);
    entry.has_file_path = sqlite3_column_type(stmt, 3) != SQLITE_NULL;
    entry.file_path = column_text(stmt, 3);
    entry.has_original_filename = sqlite3_column_type(stmt, 4) != SQLITE_NULL;
    entry.original_filename = column_text(stmt, 4);
    entry.has_mime_type = sqlite3_column_type(stmt, 5) != SQLITE_NULL;
    entry.mime_type = column_text(stmt, 5);
    entry.size_bytes = sqlite3_column_int(stmt, 6);
    entry.stored_at = sqlite3_column_int64(stmt, 7);
    entry.updated_at = sqlite3_column_int64(stmt, 8);
    entries.push_back(std::move(entry));
  }

  if (rc != SQLITE_DONE) {
    error = sqlite3_errmsg(db);
    sqlite3_finalize(stmt);
    return false;
  }

  sqlite3_finalize(stmt);
  return true;
}

bool repopulate_active_entries(sqlite3* db, const std::vector<active_entry>& entries, std::string& error) {
  sqlite3_stmt* stmt = nullptr;
  const char* sql =
      "UPDATE entries SET used=1, source_kind=?, media_kind=?, content_text=?, file_path=?, "
      "original_filename=?, mime_type=?, size_bytes=?, stored_at=?, updated_at=? WHERE id=?;";
  if (sqlite3_prepare_v2(db, sql, -1, &stmt, nullptr) != SQLITE_OK) {
    error = sqlite3_errmsg(db);
    return false;
  }

  for (size_t i = 0; i < entries.size(); ++i) {
    const auto& entry = entries[i];
    sqlite3_reset(stmt);
    sqlite3_clear_bindings(stmt);
    if (entry.has_source_kind) sqlite3_bind_text(stmt, 1, entry.source_kind.c_str(), -1, SQLITE_TRANSIENT); else sqlite3_bind_null(stmt, 1);
    if (entry.has_media_kind) sqlite3_bind_text(stmt, 2, entry.media_kind.c_str(), -1, SQLITE_TRANSIENT); else sqlite3_bind_null(stmt, 2);
    if (entry.has_content_text) sqlite3_bind_text(stmt, 3, entry.content_text.c_str(), -1, SQLITE_TRANSIENT); else sqlite3_bind_null(stmt, 3);
    if (entry.has_file_path) sqlite3_bind_text(stmt, 4, entry.file_path.c_str(), -1, SQLITE_TRANSIENT); else sqlite3_bind_null(stmt, 4);
    if (entry.has_original_filename) sqlite3_bind_text(stmt, 5, entry.original_filename.c_str(), -1, SQLITE_TRANSIENT); else sqlite3_bind_null(stmt, 5);
    if (entry.has_mime_type) sqlite3_bind_text(stmt, 6, entry.mime_type.c_str(), -1, SQLITE_TRANSIENT); else sqlite3_bind_null(stmt, 6);
    sqlite3_bind_int(stmt, 7, entry.size_bytes);
    sqlite3_bind_int64(stmt, 8, entry.stored_at);
    sqlite3_bind_int64(stmt, 9, entry.updated_at);
    sqlite3_bind_int(stmt, 10, static_cast<int>(i + 1));
    if (sqlite3_step(stmt) != SQLITE_DONE) {
      error = sqlite3_errmsg(db);
      sqlite3_finalize(stmt);
      return false;
    }
  }

  sqlite3_finalize(stmt);
  return true;
}

void remove_files(const std::vector<std::string>& paths) {
  for (const auto& path : paths) {
    if (path.empty()) continue;
    std::error_code ec;
    std::filesystem::remove(path, ec);
  }
}

bool shrink_slots(sqlite3* db, int new_max_items, bool force, std::vector<std::string>& files_to_remove, std::string& error) {
  sqlite3_stmt* stmt = nullptr;
  if (sqlite3_prepare_v2(db, "SELECT COUNT(1) FROM entries WHERE id > ? AND used = 1;", -1, &stmt, nullptr) != SQLITE_OK) {
    error = sqlite3_errmsg(db);
    return false;
  }
  sqlite3_bind_int(stmt, 1, new_max_items);
  int active_above_limit = 0;
  if (sqlite3_step(stmt) == SQLITE_ROW) active_above_limit = sqlite3_column_int(stmt, 0);
  sqlite3_finalize(stmt);

  if (active_above_limit > 0) {
    if (!force) {
      error = "shrink requires --force because active entries exist above the new limit";
      return false;
    }

    std::vector<active_entry> active_entries;
    if (!load_active_entries(db, active_entries, error)) return false;

    size_t keep_start = 0;
    if (active_entries.size() > static_cast<size_t>(new_max_items)) {
      keep_start = active_entries.size() - static_cast<size_t>(new_max_items);
      for (size_t i = 0; i < keep_start; ++i) {
        if (!active_entries[i].file_path.empty()) files_to_remove.push_back(active_entries[i].file_path);
      }
    }

    std::vector<active_entry> kept_entries;
    kept_entries.reserve(active_entries.size() - keep_start);
    for (size_t i = keep_start; i < active_entries.size(); ++i) kept_entries.push_back(std::move(active_entries[i]));

    if (!exec_stmt(db, "DELETE FROM entries;", error)) return false;
    if (!ensure_slots(db, 1, new_max_items, error)) return false;
    return repopulate_active_entries(db, kept_entries, error);
  }

  if (force) {
    std::vector<active_entry> active_entries;
    if (!load_active_entries(db, active_entries, error)) return false;
    if (!exec_stmt(db, "DELETE FROM entries;", error)) return false;
    if (!ensure_slots(db, 1, new_max_items, error)) return false;
    return repopulate_active_entries(db, active_entries, error);
  }

  if (sqlite3_prepare_v2(db, "DELETE FROM entries WHERE id > ?;", -1, &stmt, nullptr) != SQLITE_OK) {
    error = sqlite3_errmsg(db);
    return false;
  }
  sqlite3_bind_int(stmt, 1, new_max_items);
  const bool ok = sqlite3_step(stmt) == SQLITE_DONE;
  if (!ok) error = sqlite3_errmsg(db);
  sqlite3_finalize(stmt);
  return ok;
}

bool apply_resize(sqlite3* db, int requested_max_items, bool force, init_result& result, std::vector<std::string>& files_to_remove, bool& reset_next_id, std::string& error) {
  int current_max_items = result.previous_max_items == 0 ? requested_max_items : result.previous_max_items;

  if (requested_max_items > current_max_items) {
    if (!ensure_slots(db, current_max_items + 1, requested_max_items, error)) return false;
    result.resized = true;
    result.current_max_items = requested_max_items;
    return true;
  }

  if (requested_max_items < current_max_items) {
    if (!drop_fts_objects(db, error)) return false;
    if (!shrink_slots(db, requested_max_items, force, files_to_remove, error)) return false;
    result.resized = true;
    result.current_max_items = requested_max_items;
    reset_next_id = force;
    return true;
  }

  result.current_max_items = current_max_items;
  return true;
}

}  // namespace karing::db::detail

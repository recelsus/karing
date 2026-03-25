#include "karing_dao_internal.h"

#include <fstream>

namespace karing::dao {

int KaringDao::insert_text(const std::string& content) {
  detail::Db db(db_path_);
  if (!db.ok()) return -1;

  int slot_id = 0;
  int max_items = 0;
  if (!detail::fetch_slot_state(db, slot_id, max_items)) return -1;

  std::string old_file_path;
  detail::read_entry_file_path(db, slot_id, old_file_path);

  if (!detail::exec_simple(db, "BEGIN IMMEDIATE;")) return -1;

  sqlite3_stmt* stmt = nullptr;
  const char* sql =
      "UPDATE entries SET "
      "used=1, source_kind='direct_text', media_kind='text', content_text=?, file_path=NULL, "
      "original_filename=NULL, mime_type='text/plain; charset=utf-8', size_bytes=?, stored_at=?, updated_at=? "
      "WHERE id=?;";
  if (sqlite3_prepare_v2(db, sql, -1, &stmt, nullptr) != SQLITE_OK) {
    detail::exec_simple(db, "ROLLBACK;");
    return -1;
  }

  const auto ts = detail::now_epoch();
  sqlite3_bind_text(stmt, 1, content.c_str(), -1, SQLITE_TRANSIENT);
  sqlite3_bind_int64(stmt, 2, static_cast<sqlite3_int64>(content.size()));
  sqlite3_bind_int64(stmt, 3, ts);
  sqlite3_bind_int64(stmt, 4, ts);
  sqlite3_bind_int(stmt, 5, slot_id);
  const bool ok = sqlite3_step(stmt) == SQLITE_DONE && sqlite3_changes(db) > 0;
  sqlite3_finalize(stmt);

  if (!ok || !detail::advance_next_id(db, max_items) || !detail::exec_simple(db, "COMMIT;")) {
    detail::exec_simple(db, "ROLLBACK;");
    return -1;
  }

  detail::remove_file_if_any(old_file_path);
  return slot_id;
}

bool KaringDao::logical_delete(int id) {
  detail::Db db(db_path_);
  if (!db.ok()) return false;

  std::string file_path;
  KaringRecord dummy{};
  if (!detail::load_entry(db, id, dummy, &file_path, false)) return false;

  sqlite3_stmt* stmt = nullptr;
  const char* sql =
      "UPDATE entries SET "
      "used=0, source_kind=NULL, media_kind=NULL, content_text=NULL, file_path=NULL, "
      "original_filename=NULL, mime_type=NULL, size_bytes=0, stored_at=NULL, updated_at=NULL "
      "WHERE id=?;";
  if (sqlite3_prepare_v2(db, sql, -1, &stmt, nullptr) != SQLITE_OK) return false;
  sqlite3_bind_int(stmt, 1, id);
  const bool ok = sqlite3_step(stmt) == SQLITE_DONE && sqlite3_changes(db) > 0;
  sqlite3_finalize(stmt);
  if (ok) detail::remove_file_if_any(file_path);
  return ok;
}

bool KaringDao::logical_delete_latest_recent(int max_age_seconds) {
  detail::Db db(db_path_);
  if (!db.ok()) return false;

  const auto target_id = detail::previous_slot_id(db);
  if (!target_id) return false;

  sqlite3_stmt* stmt = nullptr;
  if (sqlite3_prepare_v2(db,
                         "SELECT used, stored_at FROM entries WHERE id=?;",
                         -1,
                         &stmt,
                         nullptr) != SQLITE_OK) {
    return false;
  }
  sqlite3_bind_int(stmt, 1, *target_id);

  bool can_delete = false;
  if (sqlite3_step(stmt) == SQLITE_ROW) {
    const bool used = sqlite3_column_int(stmt, 0) != 0;
    const bool has_stored_at = sqlite3_column_type(stmt, 1) != SQLITE_NULL;
    const auto stored_at = has_stored_at ? sqlite3_column_int64(stmt, 1) : 0;
    can_delete = used && has_stored_at && (detail::now_epoch() - stored_at <= max_age_seconds);
  }
  sqlite3_finalize(stmt);

  if (!can_delete) return false;
  return logical_delete(*target_id);
}

int KaringDao::insert_file(const std::string& filename, const std::string& mime, const std::string& data) {
  detail::Db db(db_path_);
  if (!db.ok()) return -1;

  int slot_id = 0;
  int max_items = 0;
  if (!detail::fetch_slot_state(db, slot_id, max_items)) return -1;

  std::string old_file_path;
  detail::read_entry_file_path(db, slot_id, old_file_path);

  std::string new_file_path;
  if (!detail::write_file_for_slot(upload_path_, slot_id, data, new_file_path)) return -1;

  if (!detail::exec_simple(db, "BEGIN IMMEDIATE;")) {
    detail::remove_file_if_any(new_file_path);
    return -1;
  }

  sqlite3_stmt* stmt = nullptr;
  const char* sql =
      "UPDATE entries SET "
      "used=1, source_kind='file_upload', media_kind=?, content_text=NULL, file_path=?, "
      "original_filename=?, mime_type=?, size_bytes=?, stored_at=?, updated_at=? "
      "WHERE id=?;";
  if (sqlite3_prepare_v2(db, sql, -1, &stmt, nullptr) != SQLITE_OK) {
    detail::exec_simple(db, "ROLLBACK;");
    detail::remove_file_if_any(new_file_path);
    return -1;
  }

  const auto ts = detail::now_epoch();
  const auto media_kind = detail::media_kind_for_mime(mime);
  sqlite3_bind_text(stmt, 1, media_kind.c_str(), -1, SQLITE_TRANSIENT);
  sqlite3_bind_text(stmt, 2, new_file_path.c_str(), -1, SQLITE_TRANSIENT);
  sqlite3_bind_text(stmt, 3, filename.c_str(), -1, SQLITE_TRANSIENT);
  sqlite3_bind_text(stmt, 4, mime.c_str(), -1, SQLITE_TRANSIENT);
  sqlite3_bind_int64(stmt, 5, static_cast<sqlite3_int64>(data.size()));
  sqlite3_bind_int64(stmt, 6, ts);
  sqlite3_bind_int64(stmt, 7, ts);
  sqlite3_bind_int(stmt, 8, slot_id);
  const bool ok = sqlite3_step(stmt) == SQLITE_DONE && sqlite3_changes(db) > 0;
  sqlite3_finalize(stmt);

  if (!ok || !detail::advance_next_id(db, max_items) || !detail::exec_simple(db, "COMMIT;")) {
    detail::exec_simple(db, "ROLLBACK;");
    detail::remove_file_if_any(new_file_path);
    return -1;
  }

  detail::remove_file_if_any(old_file_path);
  return slot_id;
}

bool KaringDao::update_text(int id, const std::string& content) {
  detail::Db db(db_path_);
  if (!db.ok()) return false;

  std::string old_file_path;
  KaringRecord current{};
  if (!detail::load_entry(db, id, current, &old_file_path)) return false;

  sqlite3_stmt* stmt = nullptr;
  const char* sql =
      "UPDATE entries SET "
      "used=1, source_kind='direct_text', media_kind='text', content_text=?, file_path=NULL, "
      "original_filename=NULL, mime_type='text/plain; charset=utf-8', size_bytes=?, updated_at=? "
      "WHERE id=?;";
  if (sqlite3_prepare_v2(db, sql, -1, &stmt, nullptr) != SQLITE_OK) return false;
  sqlite3_bind_text(stmt, 1, content.c_str(), -1, SQLITE_TRANSIENT);
  sqlite3_bind_int64(stmt, 2, static_cast<sqlite3_int64>(content.size()));
  sqlite3_bind_int64(stmt, 3, detail::now_epoch());
  sqlite3_bind_int(stmt, 4, id);
  const bool ok = sqlite3_step(stmt) == SQLITE_DONE && sqlite3_changes(db) > 0;
  sqlite3_finalize(stmt);
  if (ok) detail::remove_file_if_any(old_file_path);
  return ok;
}

bool KaringDao::update_file(int id, const std::string& filename, const std::string& mime, const std::string& data) {
  detail::Db db(db_path_);
  if (!db.ok()) return false;

  std::string old_file_path;
  KaringRecord current{};
  if (!detail::load_entry(db, id, current, &old_file_path)) return false;

  std::string new_file_path;
  if (!detail::write_file_for_slot(upload_path_, id, data, new_file_path)) return false;

  sqlite3_stmt* stmt = nullptr;
  const char* sql =
      "UPDATE entries SET "
      "used=1, source_kind='file_upload', media_kind=?, content_text=NULL, file_path=?, "
      "original_filename=?, mime_type=?, size_bytes=?, updated_at=? "
      "WHERE id=?;";
  if (sqlite3_prepare_v2(db, sql, -1, &stmt, nullptr) != SQLITE_OK) {
    detail::remove_file_if_any(new_file_path);
    return false;
  }
  const auto media_kind = detail::media_kind_for_mime(mime);
  sqlite3_bind_text(stmt, 1, media_kind.c_str(), -1, SQLITE_TRANSIENT);
  sqlite3_bind_text(stmt, 2, new_file_path.c_str(), -1, SQLITE_TRANSIENT);
  sqlite3_bind_text(stmt, 3, filename.c_str(), -1, SQLITE_TRANSIENT);
  sqlite3_bind_text(stmt, 4, mime.c_str(), -1, SQLITE_TRANSIENT);
  sqlite3_bind_int64(stmt, 5, static_cast<sqlite3_int64>(data.size()));
  sqlite3_bind_int64(stmt, 6, detail::now_epoch());
  sqlite3_bind_int(stmt, 7, id);
  const bool ok = sqlite3_step(stmt) == SQLITE_DONE && sqlite3_changes(db) > 0;
  sqlite3_finalize(stmt);
  if (!ok) {
    detail::remove_file_if_any(new_file_path);
    return false;
  }
  detail::remove_file_if_any(old_file_path);
  return true;
}

bool KaringDao::patch_text(int id, const std::optional<std::string>& content) {
  detail::Db db(db_path_);
  if (!db.ok()) return false;
  KaringRecord current{};
  std::string file_path;
  if (!detail::load_entry(db, id, current, &file_path) || current.is_file || !file_path.empty()) return false;
  return update_text(id, content.value_or(current.content));
}

bool KaringDao::patch_file(int id, const std::optional<std::string>& filename, const std::optional<std::string>& mime, const std::optional<std::string>& data) {
  detail::Db db(db_path_);
  if (!db.ok()) return false;
  KaringRecord current{};
  std::string file_path;
  if (!detail::load_entry(db, id, current, &file_path) || file_path.empty()) return false;

  std::string blob;
  if (data.has_value()) {
    blob = *data;
  } else {
    std::ifstream ifs(file_path, std::ios::binary);
    if (!ifs.is_open()) return false;
    blob.assign(std::istreambuf_iterator<char>(ifs), std::istreambuf_iterator<char>());
  }

  return update_file(id,
                     filename.value_or(current.filename),
                     mime.value_or(current.mime),
                     blob);
}

}  // namespace karing::dao

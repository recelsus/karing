#include "store/entry_store.h"

#include "dao/karing_dao_internal.h"
#include "repository/store_state_repository.h"
#include "storage/file_storage.h"

namespace karing::store {

int entry_store::insert_file(const std::string& filename, const std::string& mime, const std::string& data) const {
  dao::detail::Db db(db_path_);
  if (!db.ok()) return -1;
  storage::file_storage storage(upload_path_);

  int slot_id = 0;
  int max_items = 0;
  repository::store_state_repository store_repo(db_path_);
  if (!store_repo.fetch_state(slot_id, max_items)) return -1;
  if (!dao::detail::choose_insert_slot(db, slot_id, slot_id)) return -1;

  std::string old_file_path;
  dao::detail::read_entry_file_path(db, slot_id, old_file_path);

  std::string new_file_path;
  if (!storage.write(data, new_file_path)) return -1;

  if (!dao::detail::exec_simple(db, "BEGIN IMMEDIATE;")) {
    storage::file_storage::remove_if_any(new_file_path);
    return -1;
  }

  sqlite3_stmt* stmt = nullptr;
  const char* sql =
      "UPDATE entries SET "
      "used=1, source_kind='file_upload', media_kind=?, content_text=NULL, file_path=?, "
      "original_filename=?, mime_type=?, size_bytes=?, stored_at=?, updated_at=? "
      "WHERE id=?;";
  if (sqlite3_prepare_v2(db, sql, -1, &stmt, nullptr) != SQLITE_OK) {
    dao::detail::exec_simple(db, "ROLLBACK;");
    storage::file_storage::remove_if_any(new_file_path);
    return -1;
  }

  const auto ts = dao::detail::now_epoch();
  const auto media_kind = dao::detail::media_kind_for_mime(mime);
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

  if (!ok || !dao::detail::advance_next_id(db, slot_id) || !dao::detail::exec_simple(db, "COMMIT;")) {
    dao::detail::exec_simple(db, "ROLLBACK;");
    storage::file_storage::remove_if_any(new_file_path);
    return -1;
  }

  storage::file_storage::remove_if_any(old_file_path);
  return slot_id;
}

bool entry_store::update_file(int id, const std::string& filename, const std::string& mime, const std::string& data) const {
  dao::detail::Db db(db_path_);
  if (!db.ok()) return false;
  storage::file_storage storage(upload_path_);

  std::string old_file_path;
  dao::KaringRecord current{};
  if (!dao::detail::load_entry(db, id, current, &old_file_path)) return false;

  std::string new_file_path;
  if (!storage.write(data, new_file_path)) return false;
  if (!dao::detail::exec_simple(db, "BEGIN IMMEDIATE;")) {
    storage::file_storage::remove_if_any(new_file_path);
    return false;
  }

  sqlite3_stmt* stmt = nullptr;
  const char* sql =
      "UPDATE entries SET "
      "used=1, source_kind='file_upload', media_kind=?, content_text=NULL, file_path=?, "
      "original_filename=?, mime_type=?, size_bytes=?, updated_at=? "
      "WHERE id=?;";
  if (sqlite3_prepare_v2(db, sql, -1, &stmt, nullptr) != SQLITE_OK) {
    dao::detail::exec_simple(db, "ROLLBACK;");
    storage::file_storage::remove_if_any(new_file_path);
    return false;
  }
  const auto media_kind = dao::detail::media_kind_for_mime(mime);
  sqlite3_bind_text(stmt, 1, media_kind.c_str(), -1, SQLITE_TRANSIENT);
  sqlite3_bind_text(stmt, 2, new_file_path.c_str(), -1, SQLITE_TRANSIENT);
  sqlite3_bind_text(stmt, 3, filename.c_str(), -1, SQLITE_TRANSIENT);
  sqlite3_bind_text(stmt, 4, mime.c_str(), -1, SQLITE_TRANSIENT);
  sqlite3_bind_int64(stmt, 5, static_cast<sqlite3_int64>(data.size()));
  sqlite3_bind_int64(stmt, 6, dao::detail::now_epoch());
  sqlite3_bind_int(stmt, 7, id);
  const bool ok = sqlite3_step(stmt) == SQLITE_DONE && sqlite3_changes(db) > 0;
  sqlite3_finalize(stmt);
  if (!ok || !dao::detail::exec_simple(db, "COMMIT;")) {
    dao::detail::exec_simple(db, "ROLLBACK;");
    storage::file_storage::remove_if_any(new_file_path);
    return false;
  }
  storage::file_storage::remove_if_any(old_file_path);
  return true;
}

bool entry_store::patch_file(int id,
                             const std::optional<std::string>& filename,
                             const std::optional<std::string>& mime,
                             const std::optional<std::string>& data) const {
  dao::detail::Db db(db_path_);
  if (!db.ok()) return false;
  dao::KaringRecord current{};
  std::string file_path;
  if (!dao::detail::load_entry(db, id, current, &file_path) || file_path.empty()) return false;

  std::string blob;
  if (data.has_value()) {
    blob = *data;
  } else {
    if (!storage::file_storage::read(file_path, blob)) return false;
  }

  return update_file(id,
                     filename.value_or(current.filename),
                     mime.value_or(current.mime),
                     blob);
}

}  // namespace karing::store

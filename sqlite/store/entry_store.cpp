#include "store/entry_store.h"

#include <algorithm>
#include <optional>

#include "dao/karing_dao.h"
#include "dao/karing_dao_internal.h"
#include "repository/store_state_repository.h"
#include "storage/file_storage.h"

namespace karing::store {

entry_store::entry_store(std::string db_path, std::string upload_path)
    : db_path_(std::move(db_path)), upload_path_(std::move(upload_path)) {}

int entry_store::insert_text(const std::string& content) const {
  dao::detail::Db db(db_path_);
  if (!db.ok()) return -1;

  int slot_id = 0;
  int max_items = 0;
  repository::store_state_repository store_repo(db_path_);
  if (!store_repo.fetch_state(slot_id, max_items)) return -1;

  std::string old_file_path;
  dao::detail::read_entry_file_path(db, slot_id, old_file_path);

  if (!dao::detail::exec_simple(db, "BEGIN IMMEDIATE;")) return -1;

  sqlite3_stmt* stmt = nullptr;
  const char* sql =
      "UPDATE entries SET "
      "used=1, source_kind='direct_text', media_kind='text', content_text=?, file_path=NULL, "
      "original_filename=NULL, mime_type='text/plain; charset=utf-8', size_bytes=?, stored_at=?, updated_at=? "
      "WHERE id=?;";
  if (sqlite3_prepare_v2(db, sql, -1, &stmt, nullptr) != SQLITE_OK) {
    dao::detail::exec_simple(db, "ROLLBACK;");
    return -1;
  }

  const auto ts = dao::detail::now_epoch();
  sqlite3_bind_text(stmt, 1, content.c_str(), -1, SQLITE_TRANSIENT);
  sqlite3_bind_int64(stmt, 2, static_cast<sqlite3_int64>(content.size()));
  sqlite3_bind_int64(stmt, 3, ts);
  sqlite3_bind_int64(stmt, 4, ts);
  sqlite3_bind_int(stmt, 5, slot_id);
  const bool ok = sqlite3_step(stmt) == SQLITE_DONE && sqlite3_changes(db) > 0;
  sqlite3_finalize(stmt);

  if (!ok || !dao::detail::advance_next_id(db, max_items) || !dao::detail::exec_simple(db, "COMMIT;")) {
    dao::detail::exec_simple(db, "ROLLBACK;");
    return -1;
  }

  storage::file_storage::remove_if_any(old_file_path);
  return slot_id;
}

bool entry_store::logical_delete(int id) const {
  dao::detail::Db db(db_path_);
  if (!db.ok()) return false;

  std::string file_path;
  dao::KaringRecord dummy{};
  if (!dao::detail::load_entry(db, id, dummy, &file_path, false)) return false;

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
  if (ok) storage::file_storage::remove_if_any(file_path);
  return ok;
}

bool entry_store::logical_delete_latest_recent(int max_age_seconds) const {
  dao::detail::Db db(db_path_);
  if (!db.ok()) return false;

  repository::store_state_repository store_repo(db_path_);
  const auto target_id = store_repo.previous_slot_id();
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
    can_delete = used && has_stored_at && (dao::detail::now_epoch() - stored_at <= max_age_seconds);
  }
  sqlite3_finalize(stmt);

  if (!can_delete) return false;
  return logical_delete(*target_id);
}

int entry_store::insert_file(const std::string& filename, const std::string& mime, const std::string& data) const {
  dao::detail::Db db(db_path_);
  if (!db.ok()) return -1;
  storage::file_storage storage(upload_path_);

  int slot_id = 0;
  int max_items = 0;
  repository::store_state_repository store_repo(db_path_);
  if (!store_repo.fetch_state(slot_id, max_items)) return -1;

  std::string old_file_path;
  dao::detail::read_entry_file_path(db, slot_id, old_file_path);

  std::string new_file_path;
  if (!storage.write_for_slot(slot_id, data, new_file_path)) return -1;

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

  if (!ok || !dao::detail::advance_next_id(db, max_items) || !dao::detail::exec_simple(db, "COMMIT;")) {
    dao::detail::exec_simple(db, "ROLLBACK;");
    storage::file_storage::remove_if_any(new_file_path);
    return -1;
  }

  storage::file_storage::remove_if_any(old_file_path);
  return slot_id;
}

bool entry_store::update_text(int id, const std::string& content) const {
  dao::detail::Db db(db_path_);
  if (!db.ok()) return false;

  std::string old_file_path;
  dao::KaringRecord current{};
  if (!dao::detail::load_entry(db, id, current, &old_file_path)) return false;

  sqlite3_stmt* stmt = nullptr;
  const char* sql =
      "UPDATE entries SET "
      "used=1, source_kind='direct_text', media_kind='text', content_text=?, file_path=NULL, "
      "original_filename=NULL, mime_type='text/plain; charset=utf-8', size_bytes=?, updated_at=? "
      "WHERE id=?;";
  if (sqlite3_prepare_v2(db, sql, -1, &stmt, nullptr) != SQLITE_OK) return false;
  sqlite3_bind_text(stmt, 1, content.c_str(), -1, SQLITE_TRANSIENT);
  sqlite3_bind_int64(stmt, 2, static_cast<sqlite3_int64>(content.size()));
  sqlite3_bind_int64(stmt, 3, dao::detail::now_epoch());
  sqlite3_bind_int(stmt, 4, id);
  const bool ok = sqlite3_step(stmt) == SQLITE_DONE && sqlite3_changes(db) > 0;
  sqlite3_finalize(stmt);
  if (ok) storage::file_storage::remove_if_any(old_file_path);
  return ok;
}

bool entry_store::update_file(int id, const std::string& filename, const std::string& mime, const std::string& data) const {
  dao::detail::Db db(db_path_);
  if (!db.ok()) return false;
  storage::file_storage storage(upload_path_);

  std::string old_file_path;
  dao::KaringRecord current{};
  if (!dao::detail::load_entry(db, id, current, &old_file_path)) return false;

  std::string new_file_path;
  if (!storage.write_for_slot(id, data, new_file_path)) return false;

  sqlite3_stmt* stmt = nullptr;
  const char* sql =
      "UPDATE entries SET "
      "used=1, source_kind='file_upload', media_kind=?, content_text=NULL, file_path=?, "
      "original_filename=?, mime_type=?, size_bytes=?, updated_at=? "
      "WHERE id=?;";
  if (sqlite3_prepare_v2(db, sql, -1, &stmt, nullptr) != SQLITE_OK) {
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
  if (!ok) {
    storage::file_storage::remove_if_any(new_file_path);
    return false;
  }
  storage::file_storage::remove_if_any(old_file_path);
  return true;
}

bool entry_store::patch_text(int id, const std::optional<std::string>& content) const {
  dao::detail::Db db(db_path_);
  if (!db.ok()) return false;
  dao::KaringRecord current{};
  std::string file_path;
  if (!dao::detail::load_entry(db, id, current, &file_path) || current.is_file || !file_path.empty()) return false;
  return update_text(id, content.value_or(current.content));
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

bool entry_store::swap_entries(int id1, int id2) const {
  if (id1 == id2) return true;

  dao::detail::Db db(db_path_);
  if (!db.ok()) return false;
  if (!dao::detail::exec_simple(db, "BEGIN IMMEDIATE;")) return false;

  struct entry_state {
    bool used{false};
    std::optional<std::string> source_kind;
    std::optional<std::string> media_kind;
    std::optional<std::string> content_text;
    std::optional<std::string> file_path;
    std::optional<std::string> original_filename;
    std::optional<std::string> mime_type;
    long long size_bytes{0};
    std::optional<long long> stored_at;
    std::optional<long long> updated_at;
  };

  auto load_state = [&](int id, entry_state& out) -> bool {
    sqlite3_stmt* stmt = nullptr;
    const char* sql =
        "SELECT used, source_kind, media_kind, content_text, file_path, original_filename, "
        "mime_type, size_bytes, stored_at, updated_at "
        "FROM entries WHERE id=?;";
    if (sqlite3_prepare_v2(db, sql, -1, &stmt, nullptr) != SQLITE_OK) return false;
    sqlite3_bind_int(stmt, 1, id);
    bool ok = false;
    if (sqlite3_step(stmt) == SQLITE_ROW) {
      out.used = sqlite3_column_int(stmt, 0) != 0;
      if (sqlite3_column_type(stmt, 1) != SQLITE_NULL) out.source_kind = reinterpret_cast<const char*>(sqlite3_column_text(stmt, 1));
      if (sqlite3_column_type(stmt, 2) != SQLITE_NULL) out.media_kind = reinterpret_cast<const char*>(sqlite3_column_text(stmt, 2));
      if (sqlite3_column_type(stmt, 3) != SQLITE_NULL) out.content_text = reinterpret_cast<const char*>(sqlite3_column_text(stmt, 3));
      if (sqlite3_column_type(stmt, 4) != SQLITE_NULL) out.file_path = reinterpret_cast<const char*>(sqlite3_column_text(stmt, 4));
      if (sqlite3_column_type(stmt, 5) != SQLITE_NULL) out.original_filename = reinterpret_cast<const char*>(sqlite3_column_text(stmt, 5));
      if (sqlite3_column_type(stmt, 6) != SQLITE_NULL) out.mime_type = reinterpret_cast<const char*>(sqlite3_column_text(stmt, 6));
      out.size_bytes = sqlite3_column_int64(stmt, 7);
      if (sqlite3_column_type(stmt, 8) != SQLITE_NULL) out.stored_at = sqlite3_column_int64(stmt, 8);
      if (sqlite3_column_type(stmt, 9) != SQLITE_NULL) out.updated_at = sqlite3_column_int64(stmt, 9);
      ok = true;
    }
    sqlite3_finalize(stmt);
    return ok;
  };

  auto bind_optional_text = [](sqlite3_stmt* stmt, int index, const std::optional<std::string>& value) {
    if (value.has_value()) sqlite3_bind_text(stmt, index, value->c_str(), -1, SQLITE_TRANSIENT);
    else sqlite3_bind_null(stmt, index);
  };

  auto bind_optional_int64 = [](sqlite3_stmt* stmt, int index, const std::optional<long long>& value) {
    if (value.has_value()) sqlite3_bind_int64(stmt, index, static_cast<sqlite3_int64>(*value));
    else sqlite3_bind_null(stmt, index);
  };

  auto store_state = [&](int id, const entry_state& state) -> bool {
    sqlite3_stmt* stmt = nullptr;
    const char* sql =
        "UPDATE entries SET "
        "used=?, source_kind=?, media_kind=?, content_text=?, file_path=?, original_filename=?, "
        "mime_type=?, size_bytes=?, stored_at=?, updated_at=? "
        "WHERE id=?;";
    if (sqlite3_prepare_v2(db, sql, -1, &stmt, nullptr) != SQLITE_OK) return false;
    sqlite3_bind_int(stmt, 1, state.used ? 1 : 0);
    bind_optional_text(stmt, 2, state.source_kind);
    bind_optional_text(stmt, 3, state.media_kind);
    bind_optional_text(stmt, 4, state.content_text);
    bind_optional_text(stmt, 5, state.file_path);
    bind_optional_text(stmt, 6, state.original_filename);
    bind_optional_text(stmt, 7, state.mime_type);
    sqlite3_bind_int64(stmt, 8, static_cast<sqlite3_int64>(state.size_bytes));
    bind_optional_int64(stmt, 9, state.stored_at);
    bind_optional_int64(stmt, 10, state.updated_at);
    sqlite3_bind_int(stmt, 11, id);
    const bool ok = sqlite3_step(stmt) == SQLITE_DONE;
    sqlite3_finalize(stmt);
    return ok;
  };

  entry_state first;
  entry_state second;
  if (!load_state(id1, first) || !load_state(id2, second)) {
    dao::detail::exec_simple(db, "ROLLBACK;");
    return false;
  }

  if (!store_state(id1, second) || !store_state(id2, first) || !dao::detail::exec_simple(db, "COMMIT;")) {
    dao::detail::exec_simple(db, "ROLLBACK;");
    return false;
  }

  return true;
}

std::optional<std::pair<std::vector<karing::dao::KaringRecord>, int>> entry_store::resequence_entries() const {
  dao::detail::Db db(db_path_);
  if (!db.ok()) return std::nullopt;
  if (!dao::detail::exec_simple(db, "BEGIN IMMEDIATE;")) return std::nullopt;

  struct entry_state {
    bool used{false};
    std::optional<std::string> source_kind;
    std::optional<std::string> media_kind;
    std::optional<std::string> content_text;
    std::optional<std::string> file_path;
    std::optional<std::string> original_filename;
    std::optional<std::string> mime_type;
    long long size_bytes{0};
    std::optional<long long> stored_at;
    std::optional<long long> updated_at;
  };

  auto load_all_states = [&]() -> std::optional<std::vector<entry_state>> {
    sqlite3_stmt* stmt = nullptr;
    const char* sql =
        "SELECT used, source_kind, media_kind, content_text, file_path, original_filename, "
        "mime_type, size_bytes, stored_at, updated_at "
        "FROM entries ORDER BY id ASC;";
    if (sqlite3_prepare_v2(db, sql, -1, &stmt, nullptr) != SQLITE_OK) return std::nullopt;

    std::vector<entry_state> states;
    while (sqlite3_step(stmt) == SQLITE_ROW) {
      entry_state state;
      state.used = sqlite3_column_int(stmt, 0) != 0;
      if (sqlite3_column_type(stmt, 1) != SQLITE_NULL) state.source_kind = reinterpret_cast<const char*>(sqlite3_column_text(stmt, 1));
      if (sqlite3_column_type(stmt, 2) != SQLITE_NULL) state.media_kind = reinterpret_cast<const char*>(sqlite3_column_text(stmt, 2));
      if (sqlite3_column_type(stmt, 3) != SQLITE_NULL) state.content_text = reinterpret_cast<const char*>(sqlite3_column_text(stmt, 3));
      if (sqlite3_column_type(stmt, 4) != SQLITE_NULL) state.file_path = reinterpret_cast<const char*>(sqlite3_column_text(stmt, 4));
      if (sqlite3_column_type(stmt, 5) != SQLITE_NULL) state.original_filename = reinterpret_cast<const char*>(sqlite3_column_text(stmt, 5));
      if (sqlite3_column_type(stmt, 6) != SQLITE_NULL) state.mime_type = reinterpret_cast<const char*>(sqlite3_column_text(stmt, 6));
      state.size_bytes = sqlite3_column_int64(stmt, 7);
      if (sqlite3_column_type(stmt, 8) != SQLITE_NULL) state.stored_at = sqlite3_column_int64(stmt, 8);
      if (sqlite3_column_type(stmt, 9) != SQLITE_NULL) state.updated_at = sqlite3_column_int64(stmt, 9);
      states.push_back(std::move(state));
    }
    sqlite3_finalize(stmt);
    return states;
  };

  auto bind_optional_text = [](sqlite3_stmt* stmt, int index, const std::optional<std::string>& value) {
    if (value.has_value()) sqlite3_bind_text(stmt, index, value->c_str(), -1, SQLITE_TRANSIENT);
    else sqlite3_bind_null(stmt, index);
  };

  auto bind_optional_int64 = [](sqlite3_stmt* stmt, int index, const std::optional<long long>& value) {
    if (value.has_value()) sqlite3_bind_int64(stmt, index, static_cast<sqlite3_int64>(*value));
    else sqlite3_bind_null(stmt, index);
  };

  auto store_state = [&](int id, const entry_state& state) -> bool {
    sqlite3_stmt* stmt = nullptr;
    const char* sql =
        "UPDATE entries SET "
        "used=?, source_kind=?, media_kind=?, content_text=?, file_path=?, original_filename=?, "
        "mime_type=?, size_bytes=?, stored_at=?, updated_at=? "
        "WHERE id=?;";
    if (sqlite3_prepare_v2(db, sql, -1, &stmt, nullptr) != SQLITE_OK) return false;
    sqlite3_bind_int(stmt, 1, state.used ? 1 : 0);
    bind_optional_text(stmt, 2, state.source_kind);
    bind_optional_text(stmt, 3, state.media_kind);
    bind_optional_text(stmt, 4, state.content_text);
    bind_optional_text(stmt, 5, state.file_path);
    bind_optional_text(stmt, 6, state.original_filename);
    bind_optional_text(stmt, 7, state.mime_type);
    sqlite3_bind_int64(stmt, 8, static_cast<sqlite3_int64>(state.size_bytes));
    bind_optional_int64(stmt, 9, state.stored_at);
    bind_optional_int64(stmt, 10, state.updated_at);
    sqlite3_bind_int(stmt, 11, id);
    const bool ok = sqlite3_step(stmt) == SQLITE_DONE;
    sqlite3_finalize(stmt);
    return ok;
  };

  auto states = load_all_states();
  if (!states.has_value()) {
    dao::detail::exec_simple(db, "ROLLBACK;");
    return std::nullopt;
  }

  std::vector<std::pair<int, entry_state>> active_states;
  active_states.reserve(states->size());
  for (size_t i = 0; i < states->size(); ++i) {
    if ((*states)[i].used) active_states.emplace_back(static_cast<int>(i + 1), (*states)[i]);
  }

  std::sort(active_states.begin(), active_states.end(), [](const auto& lhs, const auto& rhs) {
    const auto left_stored = lhs.second.stored_at.value_or(0);
    const auto right_stored = rhs.second.stored_at.value_or(0);
    if (left_stored != right_stored) return left_stored < right_stored;
    return lhs.first < rhs.first;
  });

  std::vector<entry_state> resequenced(states->size());
  for (size_t i = 0; i < active_states.size(); ++i) {
    resequenced[i] = active_states[i].second;
  }

  for (size_t i = 0; i < resequenced.size(); ++i) {
    if (!store_state(static_cast<int>(i + 1), resequenced[i])) {
      dao::detail::exec_simple(db, "ROLLBACK;");
      return std::nullopt;
    }
  }

  const int next_id = active_states.size() >= resequenced.size() ? 1 : static_cast<int>(active_states.size()) + 1;

  sqlite3_stmt* next_stmt = nullptr;
  if (sqlite3_prepare_v2(db,
                         "UPDATE store_state SET next_id=?, updated_at=strftime('%s','now') WHERE singleton_id=1;",
                         -1,
                         &next_stmt,
                         nullptr) != SQLITE_OK) {
    dao::detail::exec_simple(db, "ROLLBACK;");
    return std::nullopt;
  }
  sqlite3_bind_int(next_stmt, 1, next_id);
  const bool next_ok = sqlite3_step(next_stmt) == SQLITE_DONE;
  sqlite3_finalize(next_stmt);
  if (!next_ok) {
    dao::detail::exec_simple(db, "ROLLBACK;");
    return std::nullopt;
  }

  if (!dao::detail::exec_simple(db, "INSERT INTO entries_fts(entries_fts) VALUES('rebuild');") ||
      !dao::detail::exec_simple(db, "COMMIT;")) {
    dao::detail::exec_simple(db, "ROLLBACK;");
    return std::nullopt;
  }

  std::vector<dao::KaringRecord> records;
  records.reserve(active_states.size());
  for (size_t i = 0; i < active_states.size(); ++i) {
    dao::KaringRecord record{};
    std::string ignored_path;
    if (!dao::detail::load_entry(db, static_cast<int>(i + 1), record, &ignored_path)) {
      return std::nullopt;
    }
    records.push_back(std::move(record));
  }

  return std::make_optional(std::make_pair(std::move(records), next_id));
}

}  // namespace karing::store

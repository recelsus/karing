#include "karing_dao_internal.h"

#include <fstream>

namespace karing::dao {

std::optional<int> KaringDao::latest_id() {
  detail::Db db(db_path_);
  if (!db.ok()) return std::nullopt;
  sqlite3_stmt* stmt = nullptr;
  if (sqlite3_prepare_v2(db, "SELECT id FROM entries WHERE used=1 ORDER BY stored_at DESC, id DESC LIMIT 1;", -1, &stmt, nullptr) != SQLITE_OK) {
    return std::nullopt;
  }
  std::optional<int> out;
  if (sqlite3_step(stmt) == SQLITE_ROW) out = sqlite3_column_int(stmt, 0);
  sqlite3_finalize(stmt);
  return out;
}

std::optional<KaringRecord> KaringDao::get_by_id(int id) {
  detail::Db db(db_path_);
  if (!db.ok()) return std::nullopt;
  KaringRecord record{};
  if (!detail::load_entry(db, id, record)) return std::nullopt;
  return record;
}

bool KaringDao::get_file_blob(int id, std::string& out_mime, std::string& out_filename, std::string& out_data) {
  detail::Db db(db_path_);
  if (!db.ok()) return false;
  KaringRecord record{};
  std::string file_path;
  if (!detail::load_entry(db, id, record, &file_path) || file_path.empty()) return false;

  std::ifstream ifs(file_path, std::ios::binary);
  if (!ifs.is_open()) return false;
  out_data.assign(std::istreambuf_iterator<char>(ifs), std::istreambuf_iterator<char>());
  out_mime = record.mime.empty() ? "application/octet-stream" : record.mime;
  out_filename = record.filename.empty() ? "download" : record.filename;
  return true;
}

std::vector<KaringRecord> KaringDao::list_latest(int limit, SortField sort, bool desc) {
  detail::Db db(db_path_);
  std::vector<KaringRecord> out;
  if (!db.ok()) return out;
  sqlite3_stmt* stmt = nullptr;
  const std::string sql =
      "SELECT id, media_kind, content_text, original_filename, mime_type, stored_at, updated_at "
      "FROM entries WHERE used=1" +
      detail::order_by_clause(sort, desc) +
      " LIMIT ?;";
  if (sqlite3_prepare_v2(db, sql.c_str(), -1, &stmt, nullptr) != SQLITE_OK) return out;
  sqlite3_bind_int(stmt, 1, limit);
  while (sqlite3_step(stmt) == SQLITE_ROW) {
    KaringRecord r{};
    r.id = sqlite3_column_int(stmt, 0);
    std::string media = sqlite3_column_type(stmt, 1) != SQLITE_NULL ? reinterpret_cast<const char*>(sqlite3_column_text(stmt, 1)) : "text";
    r.is_file = media != "text";
    if (const unsigned char* t = sqlite3_column_text(stmt, 2)) r.content = reinterpret_cast<const char*>(t);
    if (const unsigned char* t = sqlite3_column_text(stmt, 3)) r.filename = reinterpret_cast<const char*>(t);
    if (const unsigned char* t = sqlite3_column_text(stmt, 4)) r.mime = reinterpret_cast<const char*>(t);
    r.created_at = sqlite3_column_type(stmt, 5) != SQLITE_NULL ? sqlite3_column_int64(stmt, 5) : 0;
    if (sqlite3_column_type(stmt, 6) != SQLITE_NULL) r.updated_at = sqlite3_column_int64(stmt, 6);
    out.push_back(std::move(r));
  }
  sqlite3_finalize(stmt);
  return out;
}

bool KaringDao::try_search_fts(const std::string& fts_query, int limit, SortField sort, bool desc, std::vector<KaringRecord>& out) {
  detail::Db db(db_path_);
  if (!db.ok()) return false;
  sqlite3_stmt* stmt = nullptr;
  const std::string sql =
      "SELECT e.id, e.media_kind, e.content_text, e.original_filename, e.mime_type, e.stored_at, e.updated_at "
      "FROM entries e JOIN entries_fts f ON f.rowid = e.id "
      "WHERE e.used=1 AND entries_fts MATCH ? " +
      detail::order_by_clause(sort, desc, "e") +
      " LIMIT ?;";
  if (sqlite3_prepare_v2(db, sql.c_str(), -1, &stmt, nullptr) != SQLITE_OK) return false;
  sqlite3_bind_text(stmt, 1, fts_query.c_str(), -1, SQLITE_TRANSIENT);
  sqlite3_bind_int(stmt, 2, limit);
  while (sqlite3_step(stmt) == SQLITE_ROW) {
    KaringRecord r{};
    r.id = sqlite3_column_int(stmt, 0);
    std::string media = sqlite3_column_type(stmt, 1) != SQLITE_NULL ? reinterpret_cast<const char*>(sqlite3_column_text(stmt, 1)) : "text";
    r.is_file = media != "text";
    if (const unsigned char* t = sqlite3_column_text(stmt, 2)) r.content = reinterpret_cast<const char*>(t);
    if (const unsigned char* t = sqlite3_column_text(stmt, 3)) r.filename = reinterpret_cast<const char*>(t);
    if (const unsigned char* t = sqlite3_column_text(stmt, 4)) r.mime = reinterpret_cast<const char*>(t);
    r.created_at = sqlite3_column_type(stmt, 5) != SQLITE_NULL ? sqlite3_column_int64(stmt, 5) : 0;
    if (sqlite3_column_type(stmt, 6) != SQLITE_NULL) r.updated_at = sqlite3_column_int64(stmt, 6);
    out.push_back(std::move(r));
  }
  sqlite3_finalize(stmt);
  return true;
}

int KaringDao::count_active() {
  detail::Db db(db_path_);
  if (!db.ok()) return 0;
  sqlite3_stmt* stmt = nullptr;
  int count = 0;
  if (sqlite3_prepare_v2(db, "SELECT COUNT(1) FROM entries WHERE used=1;", -1, &stmt, nullptr) == SQLITE_OK) {
    if (sqlite3_step(stmt) == SQLITE_ROW) count = sqlite3_column_int(stmt, 0);
  }
  if (stmt) sqlite3_finalize(stmt);
  return count;
}

bool KaringDao::count_search_fts(const std::string& fts_query, long long& out) {
  detail::Db db(db_path_);
  if (!db.ok()) return false;
  sqlite3_stmt* stmt = nullptr;
  const char* sql =
      "SELECT COUNT(1) "
      "FROM entries e JOIN entries_fts f ON f.rowid = e.id "
      "WHERE e.used=1 AND entries_fts MATCH ?;";
  if (sqlite3_prepare_v2(db, sql, -1, &stmt, nullptr) != SQLITE_OK) return false;
  sqlite3_bind_text(stmt, 1, fts_query.c_str(), -1, SQLITE_TRANSIENT);
  if (sqlite3_step(stmt) == SQLITE_ROW) out = sqlite3_column_int64(stmt, 0);
  sqlite3_finalize(stmt);
  return true;
}

std::vector<KaringRecord> KaringDao::list_filtered(int limit, const Filters& f) {
  detail::Db db(db_path_);
  std::vector<KaringRecord> out;
  if (!db.ok()) return out;

  std::string sql =
      "SELECT id, media_kind, content_text, original_filename, mime_type, stored_at, updated_at "
      "FROM entries WHERE 1=1";
  if (!f.include_inactive) sql += " AND used=1";
  if (f.is_file.has_value()) sql += (*f.is_file == 1) ? " AND media_kind != 'text'" : " AND media_kind = 'text'";
  if (f.mime.has_value()) sql += " AND mime_type = ?";
  if (f.filename.has_value()) sql += " AND original_filename = ?";
  sql += detail::order_by_clause(f.sort, f.order_desc);
  sql += " LIMIT ?";

  sqlite3_stmt* stmt = nullptr;
  if (sqlite3_prepare_v2(db, sql.c_str(), -1, &stmt, nullptr) != SQLITE_OK) return out;
  int idx = 1;
  if (f.mime.has_value()) sqlite3_bind_text(stmt, idx++, f.mime->c_str(), -1, SQLITE_TRANSIENT);
  if (f.filename.has_value()) sqlite3_bind_text(stmt, idx++, f.filename->c_str(), -1, SQLITE_TRANSIENT);
  sqlite3_bind_int(stmt, idx++, limit);

  while (sqlite3_step(stmt) == SQLITE_ROW) {
    KaringRecord r{};
    r.id = sqlite3_column_int(stmt, 0);
    std::string media = sqlite3_column_type(stmt, 1) != SQLITE_NULL ? reinterpret_cast<const char*>(sqlite3_column_text(stmt, 1)) : "text";
    r.is_file = media != "text";
    if (const unsigned char* t = sqlite3_column_text(stmt, 2)) r.content = reinterpret_cast<const char*>(t);
    if (const unsigned char* t = sqlite3_column_text(stmt, 3)) r.filename = reinterpret_cast<const char*>(t);
    if (const unsigned char* t = sqlite3_column_text(stmt, 4)) r.mime = reinterpret_cast<const char*>(t);
    r.created_at = sqlite3_column_type(stmt, 5) != SQLITE_NULL ? sqlite3_column_int64(stmt, 5) : 0;
    if (sqlite3_column_type(stmt, 6) != SQLITE_NULL) r.updated_at = sqlite3_column_int64(stmt, 6);
    out.push_back(std::move(r));
  }

  sqlite3_finalize(stmt);
  return out;
}

long long KaringDao::count_filtered(const Filters& f) {
  detail::Db db(db_path_);
  if (!db.ok()) return 0;

  std::string sql = "SELECT COUNT(1) FROM entries WHERE 1=1";
  if (!f.include_inactive) sql += " AND used=1";
  if (f.is_file.has_value()) sql += (*f.is_file == 1) ? " AND media_kind != 'text'" : " AND media_kind = 'text'";
  if (f.mime.has_value()) sql += " AND mime_type = ?";
  if (f.filename.has_value()) sql += " AND original_filename = ?";

  sqlite3_stmt* stmt = nullptr;
  if (sqlite3_prepare_v2(db, sql.c_str(), -1, &stmt, nullptr) != SQLITE_OK) return 0;
  int idx = 1;
  if (f.mime.has_value()) sqlite3_bind_text(stmt, idx++, f.mime->c_str(), -1, SQLITE_TRANSIENT);
  if (f.filename.has_value()) sqlite3_bind_text(stmt, idx++, f.filename->c_str(), -1, SQLITE_TRANSIENT);

  long long count = 0;
  if (sqlite3_step(stmt) == SQLITE_ROW) count = sqlite3_column_int64(stmt, 0);
  sqlite3_finalize(stmt);
  return count;
}

}  // namespace karing::dao

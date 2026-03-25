#include "karing_dao_internal.h"

#include <chrono>
#include <filesystem>
#include <fstream>

namespace fs = std::filesystem;

namespace karing::dao {

KaringDao::KaringDao(std::string db_path, std::string upload_path)
    : db_path_(std::move(db_path)), upload_path_(std::move(upload_path)) {}

namespace detail {

Db::Db(const std::string& path) {
  sqlite3_open_v2(path.c_str(), &handle, SQLITE_OPEN_READWRITE | SQLITE_OPEN_CREATE, nullptr);
}

Db::~Db() {
  if (handle) sqlite3_close(handle);
}

Db::operator sqlite3*() {
  return handle;
}

bool Db::ok() const {
  return handle != nullptr;
}

int64_t now_epoch() {
  using namespace std::chrono;
  return duration_cast<seconds>(system_clock::now().time_since_epoch()).count();
}

const char* sort_column(SortField sort) {
  switch (sort) {
    case SortField::id:
      return "id";
    case SortField::stored_at:
      return "stored_at";
    case SortField::updated_at:
      return "updated_at";
  }
  return "stored_at";
}

std::string qualified_sort_column(SortField sort, const char* table_alias) {
  const char* column = sort_column(sort);
  if (!table_alias || !*table_alias) return column;
  return std::string(table_alias) + "." + column;
}

std::string order_by_clause(SortField sort, bool desc, const char* table_alias) {
  const std::string column = qualified_sort_column(sort, table_alias);
  return " ORDER BY " + column + (desc ? " DESC" : " ASC") + ", " +
         std::string(table_alias && *table_alias ? table_alias : "") +
         (table_alias && *table_alias ? ".id " : "id ") +
         (desc ? "DESC" : "ASC");
}

std::string media_kind_for_mime(const std::string& mime) {
  if (mime.rfind("text/", 0) == 0) return "text";
  if (mime.rfind("image/", 0) == 0) return "image";
  if (mime.rfind("audio/", 0) == 0) return "audio";
  if (mime.rfind("video/", 0) == 0) return "video";
  return "binary";
}

bool exec_simple(sqlite3* db, const char* sql) {
  char* errmsg = nullptr;
  const int rc = sqlite3_exec(db, sql, nullptr, nullptr, &errmsg);
  if (errmsg) sqlite3_free(errmsg);
  return rc == SQLITE_OK;
}

bool read_entry_file_path(sqlite3* db, int id, std::string& file_path) {
  sqlite3_stmt* stmt = nullptr;
  if (sqlite3_prepare_v2(db, "SELECT file_path FROM entries WHERE id=?;", -1, &stmt, nullptr) != SQLITE_OK) return false;
  sqlite3_bind_int(stmt, 1, id);
  bool ok = false;
  if (sqlite3_step(stmt) == SQLITE_ROW) {
    if (const unsigned char* t = sqlite3_column_text(stmt, 0)) file_path = reinterpret_cast<const char*>(t);
    ok = true;
  }
  sqlite3_finalize(stmt);
  return ok;
}

void remove_file_if_any(const std::string& path) {
  if (path.empty()) return;
  std::error_code ec;
  fs::remove(path, ec);
}

bool write_file_for_slot(const std::string& upload_root, int id, const std::string& data, std::string& out_path) {
  if (upload_root.empty()) return false;
  std::error_code ec;
  fs::create_directories(upload_root, ec);
  if (ec) return false;

  const auto stamp = std::chrono::steady_clock::now().time_since_epoch().count();
  out_path = (fs::path(upload_root) / ("entry_" + std::to_string(id) + "_" + std::to_string(stamp))).string();
  std::ofstream ofs(out_path, std::ios::binary | std::ios::trunc);
  if (!ofs.is_open()) return false;
  ofs.write(data.data(), static_cast<std::streamsize>(data.size()));
  return ofs.good();
}

bool fetch_slot_state(sqlite3* db, int& id, int& max_items) {
  sqlite3_stmt* stmt = nullptr;
  if (sqlite3_prepare_v2(db, "SELECT next_id, max_items FROM store_state WHERE singleton_id=1;", -1, &stmt, nullptr) != SQLITE_OK) return false;
  bool ok = false;
  if (sqlite3_step(stmt) == SQLITE_ROW) {
    id = sqlite3_column_int(stmt, 0);
    max_items = sqlite3_column_int(stmt, 1);
    ok = true;
  }
  sqlite3_finalize(stmt);
  return ok;
}

std::optional<int> previous_slot_id(sqlite3* db) {
  int next_id = 0;
  int max_items = 0;
  if (!fetch_slot_state(db, next_id, max_items) || max_items < 1) return std::nullopt;
  return next_id == 1 ? max_items : next_id - 1;
}

bool advance_next_id(sqlite3* db, int max_items) {
  sqlite3_stmt* stmt = nullptr;
  if (sqlite3_prepare_v2(db,
                         "UPDATE store_state SET next_id = CASE WHEN next_id >= max_items THEN 1 ELSE next_id + 1 END, "
                         "updated_at = strftime('%s','now') WHERE singleton_id=1;",
                         -1,
                         &stmt,
                         nullptr) != SQLITE_OK) {
    return false;
  }
  const bool ok = sqlite3_step(stmt) == SQLITE_DONE;
  sqlite3_finalize(stmt);
  return ok;
}

bool load_entry(sqlite3* db, int id, KaringRecord& record, std::string* file_path, bool require_used) {
  sqlite3_stmt* stmt = nullptr;
  const char* sql =
      "SELECT id, used, media_kind, content_text, original_filename, mime_type, stored_at, updated_at, file_path "
      "FROM entries WHERE id=?;";
  if (sqlite3_prepare_v2(db, sql, -1, &stmt, nullptr) != SQLITE_OK) return false;
  sqlite3_bind_int(stmt, 1, id);
  bool ok = false;
  if (sqlite3_step(stmt) == SQLITE_ROW) {
    const bool used = sqlite3_column_int(stmt, 1) != 0;
    if (!require_used || used) {
      record.id = sqlite3_column_int(stmt, 0);
      record.is_file = false;
      if (const unsigned char* t = sqlite3_column_text(stmt, 2)) {
        const std::string media = reinterpret_cast<const char*>(t);
        record.is_file = media != "text";
      }
      if (const unsigned char* t = sqlite3_column_text(stmt, 3)) record.content = reinterpret_cast<const char*>(t);
      if (const unsigned char* t = sqlite3_column_text(stmt, 4)) record.filename = reinterpret_cast<const char*>(t);
      if (const unsigned char* t = sqlite3_column_text(stmt, 5)) record.mime = reinterpret_cast<const char*>(t);
      record.created_at = sqlite3_column_type(stmt, 6) != SQLITE_NULL ? sqlite3_column_int64(stmt, 6) : 0;
      if (sqlite3_column_type(stmt, 7) != SQLITE_NULL) record.updated_at = sqlite3_column_int64(stmt, 7);
      if (file_path && sqlite3_column_type(stmt, 8) != SQLITE_NULL) *file_path = reinterpret_cast<const char*>(sqlite3_column_text(stmt, 8));
      ok = true;
    }
  }
  sqlite3_finalize(stmt);
  return ok;
}

}  // namespace detail
}  // namespace karing::dao

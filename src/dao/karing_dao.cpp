#include "karing_dao.h"
#include <sqlite3.h>
#include <chrono>
#include "utils/strings.h"
#include "utils/options.h"
#include <drogon/drogon.h>

namespace karing::dao {

namespace {
inline int64_t now_epoch() {
  using namespace std::chrono;
  return duration_cast<seconds>(system_clock::now().time_since_epoch()).count();
}

// Deactivate oldest rows beyond limit and clear payload to save space.
static void trim_active_over_limit(sqlite3* db, int limit) {
  if (!db || limit <= 0) return;
  const char* sql =
      "WITH to_trim AS (\n"
      "  SELECT id FROM karing WHERE is_active=1 ORDER BY created_at DESC, id DESC LIMIT -1 OFFSET ?\n"
      ")\n"
      "UPDATE karing SET is_active=0, content=NULL, content_blob=NULL, filename=NULL, mime=NULL\n"
      "WHERE id IN (SELECT id FROM to_trim);";
  sqlite3_stmt* st=nullptr; if (sqlite3_prepare_v2(db, sql, -1, &st, nullptr)!=SQLITE_OK) return; sqlite3_bind_int(st,1,limit); sqlite3_step(st); sqlite3_finalize(st);
}

struct Db {
  sqlite3* handle{nullptr};
  explicit Db(const std::string& path) { sqlite3_open_v2(path.c_str(), &handle, SQLITE_OPEN_READWRITE | SQLITE_OPEN_CREATE, nullptr); }
  ~Db() { if (handle) sqlite3_close(handle); }
  operator sqlite3*() { return handle; }
  bool ok() const { return handle != nullptr; }
};
}

KaringDao::KaringDao(std::string db_path) : db_path_(std::move(db_path)) {}

// preallocate_slots removed (no longer used)

int KaringDao::insert_text(const std::string& content) {
  Db db(db_path_);
  if (!db.ok()) { LOG_ERROR << "sqlite open failed at '" << db_path_ << "'"; return -1; }
  // Decide: insert new row or overwrite oldest active when reaching limit
  int active = 0; sqlite3_stmt* count_st = nullptr;
  if (sqlite3_prepare_v2(db, "SELECT COUNT(1) FROM karing WHERE is_active=1;", -1, &count_st, nullptr) == SQLITE_OK) {
    if (sqlite3_step(count_st) == SQLITE_ROW) active = sqlite3_column_int(count_st, 0);
  }
  if (count_st) sqlite3_finalize(count_st);
  int limit = karing::options::runtime_options::instance().runtime_limit();
  if (active < limit) {
    const char* sql =
        "INSERT INTO karing(content, is_file, filename, mime, content_blob, created_at, updated_at, revision, is_active)\n"
        "VALUES(?, 0, NULL, NULL, NULL, ?, NULL, 0, 1);";
    sqlite3_stmt* stmt = nullptr;
    if (sqlite3_prepare_v2(db, sql, -1, &stmt, nullptr) != SQLITE_OK) { LOG_ERROR << "prepare failed (insert_text): " << sqlite3_errmsg(db); return -1; }
    int idx = 1;
    sqlite3_bind_text(stmt, idx++, content.c_str(), -1, SQLITE_TRANSIENT);
    sqlite3_bind_int64(stmt, idx++, now_epoch());
    
    int rc = sqlite3_step(stmt);
    sqlite3_finalize(stmt);
    if (rc != SQLITE_DONE) { LOG_ERROR << "step failed (insert_text) rc=" << rc << ": " << sqlite3_errmsg(db); return -1; }
    return (int)sqlite3_last_insert_rowid(db);
  }
  // Overwrite the oldest active row (by created_at asc, id asc)
  int target_id = -1; sqlite3_stmt* pick = nullptr;
  if (sqlite3_prepare_v2(db, "SELECT id FROM karing WHERE is_active=1 ORDER BY created_at ASC, id ASC LIMIT 1;", -1, &pick, nullptr) == SQLITE_OK) {
    if (sqlite3_step(pick) == SQLITE_ROW) target_id = sqlite3_column_int(pick, 0);
  }
  if (pick) sqlite3_finalize(pick);
  if (target_id < 0) {
    // Fallback: insert if none found
    const char* sql =
        "INSERT INTO karing(content, is_file, filename, mime, content_blob, created_at, updated_at, revision, is_active)\n"
        "VALUES(?, 0, NULL, NULL, NULL, ?, NULL, 0, 1);";
    sqlite3_stmt* stmt = nullptr;
    if (sqlite3_prepare_v2(db, sql, -1, &stmt, nullptr) != SQLITE_OK) { LOG_ERROR << "prepare failed (insert_text): " << sqlite3_errmsg(db); return -1; }
    int idx = 1;
    sqlite3_bind_text(stmt, idx++, content.c_str(), -1, SQLITE_TRANSIENT);
    sqlite3_bind_int64(stmt, idx++, now_epoch());
    
    int rc = sqlite3_step(stmt);
    sqlite3_finalize(stmt);
    if (rc != SQLITE_DONE) { LOG_ERROR << "step failed (insert_text fallback) rc=" << rc << ": " << sqlite3_errmsg(db); return -1; }
    return (int)sqlite3_last_insert_rowid(db);
  }
  const char* upd =
      "UPDATE karing SET content=?, is_file=0, filename=NULL, mime=NULL, content_blob=NULL,\n"
      "created_at=?, updated_at=NULL, revision=revision+1, is_active=1 WHERE id=?;";
  sqlite3_stmt* us = nullptr;
  if (sqlite3_prepare_v2(db, upd, -1, &us, nullptr) != SQLITE_OK) { LOG_ERROR << "prepare failed (overwrite text): " << sqlite3_errmsg(db); return -1; }
  int u = 1; sqlite3_bind_text(us, u++, content.c_str(), -1, SQLITE_TRANSIENT);
  sqlite3_bind_int64(us, u++, now_epoch());
  
  sqlite3_bind_int(us, u++, target_id);
  int rc2 = sqlite3_step(us);
  sqlite3_finalize(us);
  if (rc2 != SQLITE_DONE || sqlite3_changes(db) <= 0) { LOG_ERROR << "step/changes failed (overwrite text) rc=" << rc2 << ": " << sqlite3_errmsg(db); return -1; }
  return target_id;
}

bool KaringDao::logical_delete(int id) {
  Db db(db_path_);
  if (!db.ok()) { LOG_ERROR << "sqlite open failed at '" << db_path_ << "'"; return false; }
  const char* sql = "UPDATE karing SET is_active=0, content=NULL, content_blob=NULL, filename=NULL, mime=NULL WHERE id=?;";
  sqlite3_stmt* stmt = nullptr;
  if (sqlite3_prepare_v2(db, sql, -1, &stmt, nullptr) != SQLITE_OK) { LOG_ERROR << "prepare failed (logical_delete): " << sqlite3_errmsg(db); return false; }
  sqlite3_bind_int(stmt, 1, id);
  int rc = sqlite3_step(stmt);
  sqlite3_finalize(stmt);
  return rc == SQLITE_DONE;
}

std::optional<int> KaringDao::latest_id() {
  Db db(db_path_);
  if (!db.ok()) { LOG_ERROR << "sqlite open failed at '" << db_path_ << "'"; return std::nullopt; }
  sqlite3_stmt* stmt = nullptr;
  if (sqlite3_prepare_v2(db, "SELECT id FROM karing WHERE is_active=1 ORDER BY created_at DESC, id DESC LIMIT 1;", -1, &stmt, nullptr) != SQLITE_OK) {
    LOG_ERROR << "prepare failed (latest_id): " << sqlite3_errmsg(db);
    return std::nullopt;
  }
  std::optional<int> out;
  if (sqlite3_step(stmt) == SQLITE_ROW) out = sqlite3_column_int(stmt, 0);
  sqlite3_finalize(stmt);
  return out;
}

bool KaringDao::get_file_blob(int id, std::string& out_mime, std::string& out_filename, std::string& out_data) {
  Db db(db_path_);
  if (!db.ok()) { LOG_ERROR << "sqlite open failed at '" << db_path_ << "'"; return false; }
  const char* sql = "SELECT mime, filename, content_blob FROM karing WHERE id=? AND is_active=1 AND is_file=1;";
  sqlite3_stmt* stmt = nullptr;
  if (sqlite3_prepare_v2(db, sql, -1, &stmt, nullptr) != SQLITE_OK) { LOG_ERROR << "prepare failed (get_file_blob): " << sqlite3_errmsg(db); return false; }
  sqlite3_bind_int(stmt, 1, id);
  bool ok = false;
  if (sqlite3_step(stmt) == SQLITE_ROW) {
    const unsigned char* m = sqlite3_column_text(stmt, 0);
    const unsigned char* fn = sqlite3_column_text(stmt, 1);
    const void* blob = sqlite3_column_blob(stmt, 2);
    int blen = sqlite3_column_bytes(stmt, 2);
    out_mime = m ? reinterpret_cast<const char*>(m) : "application/octet-stream";
    out_filename = fn ? reinterpret_cast<const char*>(fn) : "download";
    out_data.assign(reinterpret_cast<const char*>(blob), reinterpret_cast<const char*>(blob) + blen);
    ok = true;
  }
  sqlite3_finalize(stmt);
  return ok;
}

bool KaringDao::update_text(int id, const std::string& content) {
  Db db(db_path_);
  if (!db.ok()) { LOG_ERROR << "sqlite open failed at '" << db_path_ << "'"; return false; }
  // load old (only to ensure row exists)
  {
    sqlite3_stmt* s=nullptr;
    if (sqlite3_prepare_v2(db, "SELECT is_file FROM karing WHERE id=?;", -1, &s, nullptr)==SQLITE_OK) {
      sqlite3_bind_int(s, 1, id);
      if (sqlite3_step(s)!=SQLITE_ROW) { if (s) sqlite3_finalize(s); return false; }
    }
    if (s) sqlite3_finalize(s);
  }
  const char* sql =
      "UPDATE karing SET content=?, is_file=0, filename=NULL, mime=NULL, content_blob=NULL, "
      "updated_at=strftime('%s','now'), revision=revision+1, is_active=1 WHERE id=?;";
  sqlite3_stmt* stmt = nullptr;
  if (sqlite3_prepare_v2(db, sql, -1, &stmt, nullptr) != SQLITE_OK) { LOG_ERROR << "prepare failed (update_text): " << sqlite3_errmsg(db); return false; }
  sqlite3_bind_text(stmt, 1, content.c_str(), -1, SQLITE_TRANSIENT);
  sqlite3_bind_int(stmt, 2, id);
  int rc = sqlite3_step(stmt);
  sqlite3_finalize(stmt);
  if (rc != SQLITE_DONE || sqlite3_changes(db) <= 0) { LOG_ERROR << "step/changes failed (update_text) rc=" << rc << ": " << sqlite3_errmsg(db); return false; }
  trim_active_over_limit(db, karing::options::runtime_options::instance().runtime_limit());
  return true;
}

bool KaringDao::update_file(int id, const std::string& filename, const std::string& mime, const std::string& data) {
  Db db(db_path_);
  if (!db.ok()) { LOG_ERROR << "sqlite open failed at '" << db_path_ << "'"; return false; }
  // load old
  std::string oldFilename; std::string oldMime; std::string oldBlob;
  {
    sqlite3_stmt* s=nullptr; if (sqlite3_prepare_v2(db, "SELECT filename, mime, content_blob FROM karing WHERE id=?;", -1, &s, nullptr)==SQLITE_OK) {
      sqlite3_bind_int(s, 1, id);
      if (sqlite3_step(s)==SQLITE_ROW) {
        if (const unsigned char* t=sqlite3_column_text(s,0)) oldFilename=reinterpret_cast<const char*>(t);
        if (const unsigned char* t=sqlite3_column_text(s,1)) oldMime=reinterpret_cast<const char*>(t);
        const void* blob = sqlite3_column_blob(s,2); int bl = sqlite3_column_bytes(s,2);
        if (blob && bl>0) oldBlob.assign(reinterpret_cast<const char*>(blob), bl);
      }
    }
    if (s) sqlite3_finalize(s);
  }
  const char* sql =
      "UPDATE karing SET content=NULL, is_file=1, filename=?, mime=?, content_blob=?, "
      "updated_at=strftime('%s','now'), revision=revision+1, is_active=1 WHERE id=?;";
  sqlite3_stmt* stmt = nullptr;
  if (sqlite3_prepare_v2(db, sql, -1, &stmt, nullptr) != SQLITE_OK) { LOG_ERROR << "prepare failed (update_file): " << sqlite3_errmsg(db); return false; }
  sqlite3_bind_text(stmt, 1, filename.c_str(), -1, SQLITE_TRANSIENT);
  sqlite3_bind_text(stmt, 2, mime.c_str(), -1, SQLITE_TRANSIENT);
  sqlite3_bind_blob(stmt, 3, data.data(), (int)data.size(), SQLITE_TRANSIENT);
  sqlite3_bind_int(stmt, 4, id);
  int rc = sqlite3_step(stmt);
  sqlite3_finalize(stmt);
  if (rc != SQLITE_DONE || sqlite3_changes(db) <= 0) { LOG_ERROR << "step/changes failed (update_file) rc=" << rc << ": " << sqlite3_errmsg(db); return false; }
  trim_active_over_limit(db, karing::options::runtime_options::instance().runtime_limit());
  return true;
}

bool KaringDao::patch_text(int id, const std::optional<std::string>& content) {
  Db db(db_path_);
  if (!db.ok()) { LOG_ERROR << "sqlite open failed at '" << db_path_ << "'"; return false; }
  // Load current values
  sqlite3_stmt* stmt = nullptr;
  std::string curContent; int is_file = 0;
  if (sqlite3_prepare_v2(db, "SELECT content, is_file FROM karing WHERE id=? AND is_active=1;", -1, &stmt, nullptr) != SQLITE_OK) { LOG_ERROR << "prepare failed (patch_text load): " << sqlite3_errmsg(db); return false; }
  sqlite3_bind_int(stmt, 1, id);
  if (sqlite3_step(stmt) == SQLITE_ROW) {
    if (const unsigned char* t = sqlite3_column_text(stmt, 0)) curContent = reinterpret_cast<const char*>(t);
    is_file = sqlite3_column_int(stmt, 1);
  } else {
    sqlite3_finalize(stmt);
    return false;
  }
  sqlite3_finalize(stmt);
  if (is_file != 0) return false; // refuse to patch non-text
  return update_text(id, content.value_or(curContent));
}

bool KaringDao::patch_file(int id, const std::optional<std::string>& filename, const std::optional<std::string>& mime, const std::optional<std::string>& data) {
  Db db(db_path_);
  if (!db.ok()) { LOG_ERROR << "sqlite open failed at '" << db_path_ << "'"; return false; }
  // Load current values
  sqlite3_stmt* stmt = nullptr;
  std::string curFilename; std::string curMime; std::string curData; int is_file = 0;
  if (sqlite3_prepare_v2(db, "SELECT filename, mime, content_blob, is_file FROM karing WHERE id=? AND is_active=1;", -1, &stmt, nullptr) != SQLITE_OK) { LOG_ERROR << "prepare failed (patch_file load): " << sqlite3_errmsg(db); return false; }
  sqlite3_bind_int(stmt, 1, id);
  if (sqlite3_step(stmt) == SQLITE_ROW) {
    if (const unsigned char* t = sqlite3_column_text(stmt, 0)) curFilename = reinterpret_cast<const char*>(t);
    if (const unsigned char* t = sqlite3_column_text(stmt, 1)) curMime = reinterpret_cast<const char*>(t);
    const void* blob = sqlite3_column_blob(stmt, 2); int blen = sqlite3_column_bytes(stmt, 2);
    curData.assign(reinterpret_cast<const char*>(blob), reinterpret_cast<const char*>(blob) + blen);
    is_file = sqlite3_column_int(stmt, 3);
  } else {
    sqlite3_finalize(stmt);
    return false;
  }
  sqlite3_finalize(stmt);
  if (is_file == 0) return false; // refuse to patch non-file
  return update_file(id, filename.value_or(curFilename), mime.value_or(curMime), data.value_or(curData));
}

// restore_latest_snapshot removed
int KaringDao::insert_file(const std::string& filename,
                           const std::string& mime,
                           const std::string& data) {
  Db db(db_path_);
  if (!db.ok()) return -1;
  int active = 0; sqlite3_stmt* count_st = nullptr;
  if (sqlite3_prepare_v2(db, "SELECT COUNT(1) FROM karing WHERE is_active=1;", -1, &count_st, nullptr) == SQLITE_OK) {
    if (sqlite3_step(count_st) == SQLITE_ROW) active = sqlite3_column_int(count_st, 0);
  }
  if (count_st) sqlite3_finalize(count_st);
  int limit = karing::options::runtime_options::instance().runtime_limit();
  if (active < limit) {
    const char* sql =
        "INSERT INTO karing(content, is_file, filename, mime, content_blob, created_at, updated_at, revision, is_active)\n"
        "VALUES(NULL, 1, ?, ?, ?, ?, NULL, 0, 1);";
    sqlite3_stmt* stmt = nullptr;
    if (sqlite3_prepare_v2(db, sql, -1, &stmt, nullptr) != SQLITE_OK) return -1;
    int idx = 1;
    sqlite3_bind_text(stmt, idx++, filename.c_str(), -1, SQLITE_TRANSIENT);
    sqlite3_bind_text(stmt, idx++, mime.c_str(), -1, SQLITE_TRANSIENT);
    sqlite3_bind_blob(stmt, idx++, data.data(), (int)data.size(), SQLITE_TRANSIENT);
    sqlite3_bind_int64(stmt, idx++, now_epoch());
    
    int rc = sqlite3_step(stmt);
    sqlite3_finalize(stmt);
    if (rc != SQLITE_DONE) return -1;
    return (int)sqlite3_last_insert_rowid(db);
  }
  int target_id = -1; sqlite3_stmt* pick = nullptr;
  if (sqlite3_prepare_v2(db, "SELECT id FROM karing WHERE is_active=1 ORDER BY created_at ASC, id ASC LIMIT 1;", -1, &pick, nullptr) == SQLITE_OK) {
    if (sqlite3_step(pick) == SQLITE_ROW) target_id = sqlite3_column_int(pick, 0);
  }
  if (pick) sqlite3_finalize(pick);
  if (target_id < 0) {
    // fallback insert
    const char* sql =
        "INSERT INTO karing(content, is_file, filename, mime, content_blob, created_at, updated_at, revision, is_active)\n"
        "VALUES(NULL, 1, ?, ?, ?, ?, NULL, 0, 1);";
    sqlite3_stmt* stmt = nullptr;
    if (sqlite3_prepare_v2(db, sql, -1, &stmt, nullptr) != SQLITE_OK) return -1;
    int idx = 1;
    sqlite3_bind_text(stmt, idx++, filename.c_str(), -1, SQLITE_TRANSIENT);
    sqlite3_bind_text(stmt, idx++, mime.c_str(), -1, SQLITE_TRANSIENT);
    sqlite3_bind_blob(stmt, idx++, data.data(), (int)data.size(), SQLITE_TRANSIENT);
    sqlite3_bind_int64(stmt, idx++, now_epoch());
    int rc = sqlite3_step(stmt);
    sqlite3_finalize(stmt);
    if (rc != SQLITE_DONE) return -1;
    return (int)sqlite3_last_insert_rowid(db);
  }
  const char* upd =
      "UPDATE karing SET content=NULL, is_file=1, filename=?, mime=?, content_blob=?,\n"
      "updated_at=NULL, created_at=?, revision=revision+1, is_active=1 WHERE id=?;";
  sqlite3_stmt* us = nullptr;
  if (sqlite3_prepare_v2(db, upd, -1, &us, nullptr) != SQLITE_OK) return -1;
  int u=1; sqlite3_bind_text(us, u++, filename.c_str(), -1, SQLITE_TRANSIENT);
  sqlite3_bind_text(us, u++, mime.c_str(), -1, SQLITE_TRANSIENT);
  sqlite3_bind_blob(us, u++, data.data(), (int)data.size(), SQLITE_TRANSIENT);
  sqlite3_bind_int64(us, u++, now_epoch());
  
  sqlite3_bind_int(us, u++, target_id);
  int rc2 = sqlite3_step(us);
  sqlite3_finalize(us);
  if (rc2 != SQLITE_DONE || sqlite3_changes(db) <= 0) return -1;
  return target_id;
}

std::optional<KaringRecord> KaringDao::get_by_id(int id) {
  Db db(db_path_);
  if (!db.ok()) return std::nullopt;
  const char* sql = "SELECT id, is_file, content, filename, mime, created_at, updated_at FROM karing WHERE id=? AND is_active=1;";
  sqlite3_stmt* stmt = nullptr;
  if (sqlite3_prepare_v2(db, sql, -1, &stmt, nullptr) != SQLITE_OK) return std::nullopt;
  sqlite3_bind_int(stmt, 1, id);
  std::optional<KaringRecord> out;
  if (sqlite3_step(stmt) == SQLITE_ROW) {
    KaringRecord r{};
    r.id = sqlite3_column_int(stmt, 0);
    r.is_file = sqlite3_column_int(stmt, 1) != 0;
    if (const unsigned char* t = sqlite3_column_text(stmt, 2)) r.content = reinterpret_cast<const char*>(t);
    if (const unsigned char* t = sqlite3_column_text(stmt, 3)) r.filename = reinterpret_cast<const char*>(t);
    if (const unsigned char* t = sqlite3_column_text(stmt, 4)) r.mime = reinterpret_cast<const char*>(t);
    r.created_at = sqlite3_column_int64(stmt, 5);
    if (sqlite3_column_type(stmt, 6) != SQLITE_NULL) r.updated_at = sqlite3_column_int64(stmt, 6);
    out = r;
  }
  sqlite3_finalize(stmt);
  return out;
}

std::vector<KaringRecord> KaringDao::list_latest(int limit) {
  Db db(db_path_);
  std::vector<KaringRecord> v;
  if (!db.ok()) return v;
  const char* sql = "SELECT id, is_file, content, filename, mime, created_at, updated_at FROM karing WHERE is_active=1 ORDER BY created_at DESC, id DESC LIMIT ?;";
  sqlite3_stmt* stmt = nullptr;
  if (sqlite3_prepare_v2(db, sql, -1, &stmt, nullptr) != SQLITE_OK) return v;
  sqlite3_bind_int(stmt, 1, limit);
  while (sqlite3_step(stmt) == SQLITE_ROW) {
    KaringRecord r{};
    r.id = sqlite3_column_int(stmt, 0);
    r.is_file = sqlite3_column_int(stmt, 1) != 0;
    if (const unsigned char* t = sqlite3_column_text(stmt, 2)) r.content = reinterpret_cast<const char*>(t);
    if (const unsigned char* t = sqlite3_column_text(stmt, 3)) r.filename = reinterpret_cast<const char*>(t);
    if (const unsigned char* t = sqlite3_column_text(stmt, 4)) r.mime = reinterpret_cast<const char*>(t);
    r.created_at = sqlite3_column_int64(stmt, 5);
    if (sqlite3_column_type(stmt, 6) != SQLITE_NULL) r.updated_at = sqlite3_column_int64(stmt, 6);
  v.push_back(std::move(r));
}
sqlite3_finalize(stmt);
return v;
}


// list_latest_after removed

// search_text removed

bool KaringDao::try_search_fts(const std::string& fts_query, int limit, std::vector<KaringRecord>& v) {
  Db db(db_path_);
  if (!db.ok()) return false;
  const char* sql =
      "SELECT k.id, k.is_file, k.content, k.filename, k.mime, k.created_at, k.updated_at\n"
      "FROM karing k JOIN karing_fts f ON f.rowid = k.id\n"
      "WHERE k.is_active=1 AND karing_fts MATCH ?\n"
      "ORDER BY k.created_at DESC, k.id DESC LIMIT ?;";
  sqlite3_stmt* stmt = nullptr;
  int rc = sqlite3_prepare_v2(db, sql, -1, &stmt, nullptr);
  if (rc != SQLITE_OK) return false;
  sqlite3_bind_text(stmt, 1, fts_query.c_str(), -1, SQLITE_TRANSIENT);
  sqlite3_bind_int(stmt, 2, limit);
  while (sqlite3_step(stmt) == SQLITE_ROW) {
    KaringRecord r{};
    r.id = sqlite3_column_int(stmt, 0);
    r.is_file = sqlite3_column_int(stmt, 1) != 0;
    if (const unsigned char* t = sqlite3_column_text(stmt, 2)) r.content = reinterpret_cast<const char*>(t);
    if (const unsigned char* t = sqlite3_column_text(stmt, 3)) r.filename = reinterpret_cast<const char*>(t);
    if (const unsigned char* t = sqlite3_column_text(stmt, 4)) r.mime = reinterpret_cast<const char*>(t);
    r.created_at = sqlite3_column_int64(stmt, 5);
    if (sqlite3_column_type(stmt, 6) != SQLITE_NULL) r.updated_at = sqlite3_column_int64(stmt, 6);
    v.push_back(std::move(r));
  }
  sqlite3_finalize(stmt);
  return true;
}

// search_text_like removed





std::vector<KaringRecord> KaringDao::list_filtered(int limit, const Filters& f) {
  Db db(db_path_);
  std::vector<KaringRecord> v;
  if (!db.ok()) return v;
  std::string sql = "SELECT id, is_file, content, filename, mime, created_at, updated_at FROM karing WHERE 1=1";
  if (!f.include_inactive) sql += " AND is_active=1";
  if (f.is_file) sql += " AND is_file = ?";
  if (f.mime) sql += " AND mime = ?";
  if (f.filename) sql += " AND filename = ?";
  sql += f.order_desc ? " ORDER BY created_at DESC, id DESC" : " ORDER BY created_at ASC, id ASC";
  sql += " LIMIT ?";
  sqlite3_stmt* stmt = nullptr;
  if (sqlite3_prepare_v2(db, sql.c_str(), -1, &stmt, nullptr) != SQLITE_OK) return v;
  int idx = 1;
  if (f.is_file) sqlite3_bind_int(stmt, idx++, *f.is_file);
  if (f.mime) sqlite3_bind_text(stmt, idx++, f.mime->c_str(), -1, SQLITE_TRANSIENT);
  if (f.filename) sqlite3_bind_text(stmt, idx++, f.filename->c_str(), -1, SQLITE_TRANSIENT);
  sqlite3_bind_int(stmt, idx++, limit);
  while (sqlite3_step(stmt) == SQLITE_ROW) {
    KaringRecord r{};
    r.id = sqlite3_column_int(stmt, 0);
    r.is_file = sqlite3_column_int(stmt, 1) != 0;
    if (const unsigned char* t = sqlite3_column_text(stmt, 2)) r.content = reinterpret_cast<const char*>(t);
    if (const unsigned char* t = sqlite3_column_text(stmt, 3)) r.filename = reinterpret_cast<const char*>(t);
    if (const unsigned char* t = sqlite3_column_text(stmt, 4)) r.mime = reinterpret_cast<const char*>(t);
    r.created_at = sqlite3_column_int64(stmt, 5);
    if (sqlite3_column_type(stmt, 6) != SQLITE_NULL) r.updated_at = sqlite3_column_int64(stmt, 6);
    v.push_back(std::move(r));
  }
  sqlite3_finalize(stmt);
  return v;
}

long long KaringDao::count_filtered(const Filters& f) {
  Db db(db_path_);
  if (!db.ok()) return 0;
  std::string sql = "SELECT COUNT(1) FROM karing WHERE 1=1";
  if (!f.include_inactive) sql += " AND is_active=1";
  if (f.is_file) sql += " AND is_file = ?";
  if (f.mime) sql += " AND mime = ?";
  if (f.filename) sql += " AND filename = ?";
  sqlite3_stmt* stmt = nullptr;
  if (sqlite3_prepare_v2(db, sql.c_str(), -1, &stmt, nullptr) != SQLITE_OK) return 0;
  int idx = 1;
  if (f.is_file) sqlite3_bind_int(stmt, idx++, *f.is_file);
  if (f.mime) sqlite3_bind_text(stmt, idx++, f.mime->c_str(), -1, SQLITE_TRANSIENT);
  if (f.filename) sqlite3_bind_text(stmt, idx++, f.filename->c_str(), -1, SQLITE_TRANSIENT);
  long long c = 0;
  if (sqlite3_step(stmt) == SQLITE_ROW) c = sqlite3_column_int64(stmt, 0);
  sqlite3_finalize(stmt);
  return c;
}

bool KaringDao::count_search_fts(const std::string& fts_query, long long& out) {
  Db db(db_path_);
  if (!db.ok()) return false;
  const char* sql =
      "SELECT COUNT(1)\n"
      "FROM karing k JOIN karing_fts f ON f.rowid = k.id\n"
      "WHERE k.is_active=1 AND karing_fts MATCH ?;";
  sqlite3_stmt* stmt = nullptr;
  if (sqlite3_prepare_v2(db, sql, -1, &stmt, nullptr) != SQLITE_OK) return false;
  sqlite3_bind_text(stmt, 1, fts_query.c_str(), -1, SQLITE_TRANSIENT);
  long long c = 0;
  if (sqlite3_step(stmt) == SQLITE_ROW) c = sqlite3_column_int64(stmt, 0);
  sqlite3_finalize(stmt);
  out = c;
  return true;
}

// count_search_like removed


// try_search_fts_after removed

// search_text_like_after removed

int KaringDao::count_active() {
  Db db(db_path_);
  if (!db.ok()) return 0;
  sqlite3_stmt* stmt = nullptr;
  int c = 0;
  if (sqlite3_prepare_v2(db, "SELECT COUNT(1) FROM karing WHERE is_active=1;", -1, &stmt, nullptr) == SQLITE_OK) {
    if (sqlite3_step(stmt) == SQLITE_ROW) c = sqlite3_column_int(stmt, 0);
  }
  if (stmt) sqlite3_finalize(stmt);
  return c;
}

}

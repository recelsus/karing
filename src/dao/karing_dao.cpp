#include "karing_dao.h"
#include <sqlite3.h>
#include <chrono>
#include "utils/strings.h"

namespace karing::dao {

namespace {
inline int64_t now_epoch() {
  using namespace std::chrono;
  return duration_cast<seconds>(system_clock::now().time_since_epoch()).count();
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

bool KaringDao::preallocate_slots(int n) {
  Db db(db_path_);
  if (!db.ok()) return false;
  sqlite3_exec(db, "BEGIN;", nullptr, nullptr, nullptr);
  int total = 0;
  sqlite3_stmt* stmt = nullptr;
  if (sqlite3_prepare_v2(db, "SELECT COUNT(1) FROM karing;", -1, &stmt, nullptr) == SQLITE_OK) {
    if (sqlite3_step(stmt) == SQLITE_ROW) total = sqlite3_column_int(stmt, 0);
  }
  if (stmt) sqlite3_finalize(stmt);
  for (int i = total; i < n; ++i) {
    sqlite3_exec(db, "INSERT INTO karing(is_active, revision) VALUES(0, 0);", nullptr, nullptr, nullptr);
  }
  sqlite3_exec(db, "COMMIT;", nullptr, nullptr, nullptr);
  return true;
}

int KaringDao::insert_text(const std::string& content,
                           const std::string& syntax,
                           const std::string& key,
                           const std::string& client_ip,
                           const std::optional<int>& api_key_id) {
  Db db(db_path_);
  if (!db.ok()) return -1;
  sqlite3_exec(db, "BEGIN;", nullptr, nullptr, nullptr);

  int target_id = -1;
  sqlite3_stmt* stmt = nullptr;
  bool overwrote = false;
  // pick an inactive slot first
  if (sqlite3_prepare_v2(db, "SELECT id FROM karing WHERE is_active=0 ORDER BY id LIMIT 1;", -1, &stmt, nullptr) == SQLITE_OK) {
    if (sqlite3_step(stmt) == SQLITE_ROW) target_id = sqlite3_column_int(stmt, 0);
  }
  if (stmt) { sqlite3_finalize(stmt); stmt = nullptr; }

  if (target_id < 0) {
    // rotate oldest active
    if (sqlite3_prepare_v2(db, "SELECT id FROM karing WHERE is_active=1 ORDER BY created_at ASC, id ASC LIMIT 1;", -1, &stmt, nullptr) == SQLITE_OK) {
      if (sqlite3_step(stmt) == SQLITE_ROW) target_id = sqlite3_column_int(stmt, 0);
    }
    if (stmt) { sqlite3_finalize(stmt); stmt = nullptr; }
    if (target_id >= 0) overwrote = true;
  }

  if (target_id < 0) {
    // as a fallback, insert a new row
    sqlite3_exec(db, "INSERT INTO karing(is_active, revision) VALUES(0, 0);", nullptr, nullptr, nullptr);
    target_id = (int)sqlite3_last_insert_rowid(db);
  }

  // update the row
  const char* sql =
      "UPDATE karing SET key=?, content=?, syntax=?, is_file=0, filename=NULL, mime=NULL, content_blob=NULL, "
      "created_at=?, updated_at=NULL, client_ip=?, api_key_id=?, revision=0, is_active=1 WHERE id=?;";
  if (sqlite3_prepare_v2(db, sql, -1, &stmt, nullptr) != SQLITE_OK) {
    sqlite3_exec(db, "ROLLBACK;", nullptr, nullptr, nullptr);
    return -1;
  }
  int idx = 1;
  if (!key.empty()) sqlite3_bind_text(stmt, idx++, key.c_str(), -1, SQLITE_TRANSIENT); else sqlite3_bind_null(stmt, idx++);
  sqlite3_bind_text(stmt, idx++, content.c_str(), -1, SQLITE_TRANSIENT);
  if (!syntax.empty()) sqlite3_bind_text(stmt, idx++, syntax.c_str(), -1, SQLITE_TRANSIENT); else sqlite3_bind_null(stmt, idx++);
  sqlite3_bind_int64(stmt, idx++, now_epoch());
  if (!client_ip.empty()) sqlite3_bind_text(stmt, idx++, client_ip.c_str(), -1, SQLITE_TRANSIENT); else sqlite3_bind_null(stmt, idx++);
  if (api_key_id.has_value()) sqlite3_bind_int(stmt, idx++, *api_key_id); else sqlite3_bind_null(stmt, idx++);
  sqlite3_bind_int(stmt, idx++, target_id);

  int rc = sqlite3_step(stmt);
  sqlite3_finalize(stmt);
  if (rc != SQLITE_DONE) {
    sqlite3_exec(db, "ROLLBACK;", nullptr, nullptr, nullptr);
    return -1;
  }

  sqlite3_exec(db, "COMMIT;", nullptr, nullptr, nullptr);
  return target_id;
}

bool KaringDao::logical_delete(int id) {
  Db db(db_path_);
  if (!db.ok()) return false;
  const char* sql = "UPDATE karing SET is_active=0, content=NULL, content_blob=NULL, filename=NULL, mime=NULL WHERE id=?;";
  sqlite3_stmt* stmt = nullptr;
  if (sqlite3_prepare_v2(db, sql, -1, &stmt, nullptr) != SQLITE_OK) return false;
  sqlite3_bind_int(stmt, 1, id);
  int rc = sqlite3_step(stmt);
  sqlite3_finalize(stmt);
  return rc == SQLITE_DONE;
}

std::optional<int> KaringDao::latest_id() {
  Db db(db_path_);
  if (!db.ok()) return std::nullopt;
  sqlite3_stmt* stmt = nullptr;
  if (sqlite3_prepare_v2(db, "SELECT id FROM karing WHERE is_active=1 ORDER BY created_at DESC, id DESC LIMIT 1;", -1, &stmt, nullptr) != SQLITE_OK) {
    return std::nullopt;
  }
  std::optional<int> out;
  if (sqlite3_step(stmt) == SQLITE_ROW) out = sqlite3_column_int(stmt, 0);
  sqlite3_finalize(stmt);
  return out;
}

bool KaringDao::get_file_blob(int id, std::string& out_mime, std::string& out_filename, std::string& out_data) {
  Db db(db_path_);
  if (!db.ok()) return false;
  const char* sql = "SELECT mime, filename, content_blob FROM karing WHERE id=? AND is_active=1 AND is_file=1;";
  sqlite3_stmt* stmt = nullptr;
  if (sqlite3_prepare_v2(db, sql, -1, &stmt, nullptr) != SQLITE_OK) return false;
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

bool KaringDao::update_text(int id, const std::string& content, const std::string& syntax,
                            const std::string& client_ip, const std::optional<int>& api_key_id) {
  Db db(db_path_);
  if (!db.ok()) return false;
  // load old
  std::string oldContent; std::string oldSyntax; int oldIsFile=0;
  {
    sqlite3_stmt* s=nullptr;
    if (sqlite3_prepare_v2(db, "SELECT content, syntax, is_file FROM karing WHERE id=?;", -1, &s, nullptr)==SQLITE_OK) {
      sqlite3_bind_int(s, 1, id);
      if (sqlite3_step(s)==SQLITE_ROW) {
        if (const unsigned char* t=sqlite3_column_text(s,0)) oldContent=reinterpret_cast<const char*>(t);
        if (const unsigned char* t=sqlite3_column_text(s,1)) oldSyntax=reinterpret_cast<const char*>(t);
        oldIsFile=sqlite3_column_int(s,2);
      }
    }
    if (s) sqlite3_finalize(s);
  }
  const char* sql =
      "UPDATE karing SET key=key, content=?, syntax=?, is_file=0, filename=NULL, mime=NULL, content_blob=NULL, "
      "updated_at=strftime('%s','now'), revision=revision+1, is_active=1 WHERE id=?;";
  sqlite3_stmt* stmt = nullptr;
  if (sqlite3_prepare_v2(db, sql, -1, &stmt, nullptr) != SQLITE_OK) return false;
  sqlite3_bind_text(stmt, 1, content.c_str(), -1, SQLITE_TRANSIENT);
  if (!syntax.empty()) sqlite3_bind_text(stmt, 2, syntax.c_str(), -1, SQLITE_TRANSIENT); else sqlite3_bind_null(stmt, 2);
  sqlite3_bind_int(stmt, 3, id);
  int rc = sqlite3_step(stmt);
  sqlite3_finalize(stmt);
  if (rc != SQLITE_DONE || sqlite3_changes(db) <= 0) return false;
  // audit
  sqlite3_stmt* lw = nullptr;
  if (sqlite3_prepare_v2(db, "INSERT INTO overwrite_log(replaced_rowid, at, by_api_key_id, from_ip, action, fields, text_len_before, text_hash_before, text_excerpt_before, content_before, syntax_before, text_patch) VALUES(?, strftime('%s','now'), ?, ?, 'update', '[\"content\",\"syntax\"]', ?, ?, ?, ?, ?, ?);", -1, &lw, nullptr) == SQLITE_OK) {
    sqlite3_bind_int(lw, 1, id);
    if (api_key_id.has_value()) sqlite3_bind_int(lw, 2, *api_key_id); else sqlite3_bind_null(lw, 2);
    if (!client_ip.empty()) sqlite3_bind_text(lw, 3, client_ip.c_str(), -1, SQLITE_TRANSIENT); else sqlite3_bind_null(lw, 3);
    std::string excerpt = karing::str::utf8_prefix(oldContent, 200);
    std::string hash = karing::str::sha256_hex(oldContent);
    sqlite3_bind_int(lw, 4, (int)oldContent.size());
    sqlite3_bind_text(lw, 5, hash.c_str(), -1, SQLITE_TRANSIENT);
    sqlite3_bind_text(lw, 6, excerpt.c_str(), -1, SQLITE_TRANSIENT);
    if (!oldContent.empty()) sqlite3_bind_text(lw, 7, oldContent.c_str(), -1, SQLITE_TRANSIENT); else sqlite3_bind_null(lw, 7);
    if (!oldSyntax.empty()) sqlite3_bind_text(lw, 8, oldSyntax.c_str(), -1, SQLITE_TRANSIENT); else sqlite3_bind_null(lw, 8);
    std::string patch = karing::str::simple_diff(oldContent, content);
    if (!patch.empty()) sqlite3_bind_text(lw, 9, patch.c_str(), -1, SQLITE_TRANSIENT); else sqlite3_bind_null(lw, 9);
    sqlite3_step(lw);
  }
  if (lw) sqlite3_finalize(lw);
  return true;
}

bool KaringDao::update_file(int id, const std::string& filename, const std::string& mime, const std::string& data,
                            const std::string& client_ip, const std::optional<int>& api_key_id) {
  Db db(db_path_);
  if (!db.ok()) return false;
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
      "UPDATE karing SET key=key, content=NULL, syntax=NULL, is_file=1, filename=?, mime=?, content_blob=?, "
      "updated_at=strftime('%s','now'), revision=revision+1, is_active=1 WHERE id=?;";
  sqlite3_stmt* stmt = nullptr;
  if (sqlite3_prepare_v2(db, sql, -1, &stmt, nullptr) != SQLITE_OK) return false;
  sqlite3_bind_text(stmt, 1, filename.c_str(), -1, SQLITE_TRANSIENT);
  sqlite3_bind_text(stmt, 2, mime.c_str(), -1, SQLITE_TRANSIENT);
  sqlite3_bind_blob(stmt, 3, data.data(), (int)data.size(), SQLITE_TRANSIENT);
  sqlite3_bind_int(stmt, 4, id);
  int rc = sqlite3_step(stmt);
  sqlite3_finalize(stmt);
  if (rc != SQLITE_DONE || sqlite3_changes(db) <= 0) return false;
  // audit
  sqlite3_stmt* lw = nullptr;
  if (sqlite3_prepare_v2(db, "INSERT INTO overwrite_log(replaced_rowid, at, by_api_key_id, from_ip, action, fields, file_hash_before, file_len_before, mime_before, filename_before) VALUES(?, strftime('%s','now'), ?, ?, 'update', '[\"content_blob\",\"mime\",\"filename\"]', ?, ?, ?, ?);", -1, &lw, nullptr) == SQLITE_OK) {
    sqlite3_bind_int(lw, 1, id);
    if (api_key_id.has_value()) sqlite3_bind_int(lw, 2, *api_key_id); else sqlite3_bind_null(lw, 2);
    if (!client_ip.empty()) sqlite3_bind_text(lw, 3, client_ip.c_str(), -1, SQLITE_TRANSIENT); else sqlite3_bind_null(lw, 3);
    std::string hash = oldBlob.empty()? std::string(): karing::str::sha256_hex(oldBlob);
    if (!hash.empty()) sqlite3_bind_text(lw, 4, hash.c_str(), -1, SQLITE_TRANSIENT); else sqlite3_bind_null(lw, 4);
    sqlite3_bind_int(lw, 5, (int)oldBlob.size());
    if (!oldMime.empty()) sqlite3_bind_text(lw, 6, oldMime.c_str(), -1, SQLITE_TRANSIENT); else sqlite3_bind_null(lw, 6);
    if (!oldFilename.empty()) sqlite3_bind_text(lw, 7, oldFilename.c_str(), -1, SQLITE_TRANSIENT); else sqlite3_bind_null(lw, 7);
    sqlite3_step(lw);
  }
  if (lw) sqlite3_finalize(lw);
  return true;
}

bool KaringDao::patch_text(int id, const std::optional<std::string>& content, const std::optional<std::string>& syntax,
                           const std::string& client_ip, const std::optional<int>& api_key_id) {
  Db db(db_path_);
  if (!db.ok()) return false;
  // Load current values
  sqlite3_stmt* stmt = nullptr;
  std::string curContent; std::string curSyntax; int is_file = 0;
  if (sqlite3_prepare_v2(db, "SELECT content, syntax, is_file FROM karing WHERE id=? AND is_active=1;", -1, &stmt, nullptr) != SQLITE_OK) return false;
  sqlite3_bind_int(stmt, 1, id);
  if (sqlite3_step(stmt) == SQLITE_ROW) {
    if (const unsigned char* t = sqlite3_column_text(stmt, 0)) curContent = reinterpret_cast<const char*>(t);
    if (const unsigned char* t = sqlite3_column_text(stmt, 1)) curSyntax = reinterpret_cast<const char*>(t);
    is_file = sqlite3_column_int(stmt, 2);
  } else {
    sqlite3_finalize(stmt);
    return false;
  }
  sqlite3_finalize(stmt);
  if (is_file != 0) return false; // refuse to patch non-text
  return update_text(id, content.value_or(curContent), syntax.value_or(curSyntax), client_ip, api_key_id);
}

bool KaringDao::patch_file(int id, const std::optional<std::string>& filename, const std::optional<std::string>& mime, const std::optional<std::string>& data,
                           const std::string& client_ip, const std::optional<int>& api_key_id) {
  Db db(db_path_);
  if (!db.ok()) return false;
  // Load current values
  sqlite3_stmt* stmt = nullptr;
  std::string curFilename; std::string curMime; std::string curData; int is_file = 0;
  if (sqlite3_prepare_v2(db, "SELECT filename, mime, content_blob, is_file FROM karing WHERE id=? AND is_active=1;", -1, &stmt, nullptr) != SQLITE_OK) return false;
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
  return update_file(id, filename.value_or(curFilename), mime.value_or(curMime), data.value_or(curData), client_ip, api_key_id);
}

bool KaringDao::restore_latest_snapshot(int id) {
  Db db(db_path_);
  if (!db.ok()) return false;
  // find latest snapshot with content_before
  const char* q = "SELECT content_before, syntax_before FROM overwrite_log WHERE replaced_rowid=? AND content_before IS NOT NULL ORDER BY at DESC, id DESC LIMIT 1;";
  sqlite3_stmt* st=nullptr; if (sqlite3_prepare_v2(db, q, -1, &st, nullptr)!=SQLITE_OK) return false;
  sqlite3_bind_int(st, 1, id);
  std::string content, syntax;
  bool ok = false;
  if (sqlite3_step(st)==SQLITE_ROW) {
    if (const unsigned char* t = sqlite3_column_text(st, 0)) content = reinterpret_cast<const char*>(t);
    if (sqlite3_column_type(st, 1)!=SQLITE_NULL) { if (const unsigned char* t = sqlite3_column_text(st, 1)) syntax = reinterpret_cast<const char*>(t); }
    ok = !content.empty() || sqlite3_column_type(st,1)!=SQLITE_NULL;
  }
  sqlite3_finalize(st);
  if (!ok) return false;
  // apply restore
  const char* u = "UPDATE karing SET content=?, syntax=?, is_file=0, filename=NULL, mime=NULL, content_blob=NULL, updated_at=strftime('%s','now'), revision=revision+1, is_active=1 WHERE id=?;";
  sqlite3_stmt* us=nullptr; if (sqlite3_prepare_v2(db, u, -1, &us, nullptr)!=SQLITE_OK) return false;
  sqlite3_bind_text(us, 1, content.c_str(), -1, SQLITE_TRANSIENT);
  if (!syntax.empty()) sqlite3_bind_text(us, 2, syntax.c_str(), -1, SQLITE_TRANSIENT); else sqlite3_bind_null(us, 2);
  sqlite3_bind_int(us, 3, id);
  int rc = sqlite3_step(us);
  sqlite3_finalize(us);
  return rc == SQLITE_DONE && sqlite3_changes(db) > 0;
}
int KaringDao::insert_file(const std::string& filename,
                           const std::string& mime,
                           const std::string& data,
                           const std::string& key,
                           const std::string& client_ip,
                           const std::optional<int>& api_key_id) {
  Db db(db_path_);
  if (!db.ok()) return -1;
  sqlite3_exec(db, "BEGIN;", nullptr, nullptr, nullptr);
  int target_id = -1;
  sqlite3_stmt* stmt = nullptr;
  bool overwrote = false;
  if (sqlite3_prepare_v2(db, "SELECT id FROM karing WHERE is_active=0 ORDER BY id LIMIT 1;", -1, &stmt, nullptr) == SQLITE_OK) {
    if (sqlite3_step(stmt) == SQLITE_ROW) target_id = sqlite3_column_int(stmt, 0);
  }
  if (stmt) { sqlite3_finalize(stmt); stmt = nullptr; }
  if (target_id < 0) {
    if (sqlite3_prepare_v2(db, "SELECT id FROM karing WHERE is_active=1 ORDER BY created_at ASC, id ASC LIMIT 1;", -1, &stmt, nullptr) == SQLITE_OK) {
      if (sqlite3_step(stmt) == SQLITE_ROW) target_id = sqlite3_column_int(stmt, 0);
    }
    if (stmt) { sqlite3_finalize(stmt); stmt = nullptr; }
    if (target_id >= 0) overwrote = true;
  }
  if (target_id < 0) {
    sqlite3_exec(db, "INSERT INTO karing(is_active, revision) VALUES(0, 0);", nullptr, nullptr, nullptr);
    target_id = (int)sqlite3_last_insert_rowid(db);
  }

  const char* sql =
      "UPDATE karing SET key=?, content=NULL, syntax=NULL, is_file=1, filename=?, mime=?, content_blob=?, "
      "created_at=?, updated_at=NULL, client_ip=?, api_key_id=?, revision=0, is_active=1 WHERE id=?;";
  if (sqlite3_prepare_v2(db, sql, -1, &stmt, nullptr) != SQLITE_OK) {
    sqlite3_exec(db, "ROLLBACK;", nullptr, nullptr, nullptr);
    return -1;
  }
  int idx = 1;
  if (!key.empty()) sqlite3_bind_text(stmt, idx++, key.c_str(), -1, SQLITE_TRANSIENT); else sqlite3_bind_null(stmt, idx++);
  sqlite3_bind_text(stmt, idx++, filename.c_str(), -1, SQLITE_TRANSIENT);
  sqlite3_bind_text(stmt, idx++, mime.c_str(), -1, SQLITE_TRANSIENT);
  sqlite3_bind_blob(stmt, idx++, data.data(), (int)data.size(), SQLITE_TRANSIENT);
  sqlite3_bind_int64(stmt, idx++, now_epoch());
  if (!client_ip.empty()) sqlite3_bind_text(stmt, idx++, client_ip.c_str(), -1, SQLITE_TRANSIENT); else sqlite3_bind_null(stmt, idx++);
  if (api_key_id.has_value()) sqlite3_bind_int(stmt, idx++, *api_key_id); else sqlite3_bind_null(stmt, idx++);
  sqlite3_bind_int(stmt, idx++, target_id);
  int rc = sqlite3_step(stmt);
  sqlite3_finalize(stmt);
  if (rc != SQLITE_DONE) {
    sqlite3_exec(db, "ROLLBACK;", nullptr, nullptr, nullptr);
    return -1;
  }
  sqlite3_exec(db, "COMMIT;", nullptr, nullptr, nullptr);
  return target_id;
}

std::optional<KaringRecord> KaringDao::get_by_id(int id) {
  Db db(db_path_);
  if (!db.ok()) return std::nullopt;
  const char* sql = "SELECT id, is_file, key, content, filename, mime, created_at, updated_at FROM karing WHERE id=? AND is_active=1;";
  sqlite3_stmt* stmt = nullptr;
  if (sqlite3_prepare_v2(db, sql, -1, &stmt, nullptr) != SQLITE_OK) return std::nullopt;
  sqlite3_bind_int(stmt, 1, id);
  std::optional<KaringRecord> out;
  if (sqlite3_step(stmt) == SQLITE_ROW) {
    KaringRecord r{};
    r.id = sqlite3_column_int(stmt, 0);
    r.is_file = sqlite3_column_int(stmt, 1) != 0;
    if (const unsigned char* t = sqlite3_column_text(stmt, 2)) r.key = reinterpret_cast<const char*>(t);
    if (const unsigned char* t = sqlite3_column_text(stmt, 3)) r.content = reinterpret_cast<const char*>(t);
    if (const unsigned char* t = sqlite3_column_text(stmt, 4)) r.filename = reinterpret_cast<const char*>(t);
    if (const unsigned char* t = sqlite3_column_text(stmt, 5)) r.mime = reinterpret_cast<const char*>(t);
    r.created_at = sqlite3_column_int64(stmt, 6);
    if (sqlite3_column_type(stmt, 7) != SQLITE_NULL) r.updated_at = sqlite3_column_int64(stmt, 7);
    out = r;
  }
  sqlite3_finalize(stmt);
  return out;
}

std::vector<KaringRecord> KaringDao::list_latest(int limit, int offset) {
  Db db(db_path_);
  std::vector<KaringRecord> v;
  if (!db.ok()) return v;
  const char* sql = "SELECT id, is_file, key, content, filename, mime, created_at, updated_at FROM karing WHERE is_active=1 ORDER BY created_at DESC, id DESC LIMIT ? OFFSET ?;";
  sqlite3_stmt* stmt = nullptr;
  if (sqlite3_prepare_v2(db, sql, -1, &stmt, nullptr) != SQLITE_OK) return v;
  sqlite3_bind_int(stmt, 1, limit);
  sqlite3_bind_int(stmt, 2, offset);
  while (sqlite3_step(stmt) == SQLITE_ROW) {
    KaringRecord r{};
    r.id = sqlite3_column_int(stmt, 0);
    r.is_file = sqlite3_column_int(stmt, 1) != 0;
    if (const unsigned char* t = sqlite3_column_text(stmt, 2)) r.key = reinterpret_cast<const char*>(t);
    if (const unsigned char* t = sqlite3_column_text(stmt, 3)) r.content = reinterpret_cast<const char*>(t);
    if (const unsigned char* t = sqlite3_column_text(stmt, 4)) r.filename = reinterpret_cast<const char*>(t);
    if (const unsigned char* t = sqlite3_column_text(stmt, 5)) r.mime = reinterpret_cast<const char*>(t);
  r.created_at = sqlite3_column_int64(stmt, 6);
  if (sqlite3_column_type(stmt, 7) != SQLITE_NULL) r.updated_at = sqlite3_column_int64(stmt, 7);
  v.push_back(std::move(r));
}
sqlite3_finalize(stmt);
return v;
}


std::vector<KaringRecord> KaringDao::list_latest_after(int limit, long long cursor_ts, int cursor_id) {
  Db db(db_path_);
  std::vector<KaringRecord> v;
  if (!db.ok()) return v;
  const char* sql =
      "SELECT id, is_file, key, content, filename, mime, created_at, updated_at\n"
      "FROM karing\n"
      "WHERE is_active=1 AND (created_at < ? OR (created_at = ? AND id < ?))\n"
      "ORDER BY created_at DESC, id DESC LIMIT ?;";
  sqlite3_stmt* stmt = nullptr;
  if (sqlite3_prepare_v2(db, sql, -1, &stmt, nullptr) != SQLITE_OK) return v;
  sqlite3_bind_int64(stmt, 1, cursor_ts);
  sqlite3_bind_int64(stmt, 2, cursor_ts);
  sqlite3_bind_int(stmt, 3, cursor_id);
  sqlite3_bind_int(stmt, 4, limit);
  while (sqlite3_step(stmt) == SQLITE_ROW) {
    KaringRecord r{};
    r.id = sqlite3_column_int(stmt, 0);
    r.is_file = sqlite3_column_int(stmt, 1) != 0;
    if (const unsigned char* t = sqlite3_column_text(stmt, 2)) r.key = reinterpret_cast<const char*>(t);
    if (const unsigned char* t = sqlite3_column_text(stmt, 3)) r.content = reinterpret_cast<const char*>(t);
    if (const unsigned char* t = sqlite3_column_text(stmt, 4)) r.filename = reinterpret_cast<const char*>(t);
    if (const unsigned char* t = sqlite3_column_text(stmt, 5)) r.mime = reinterpret_cast<const char*>(t);
    r.created_at = sqlite3_column_int64(stmt, 6);
    if (sqlite3_column_type(stmt, 7) != SQLITE_NULL) r.updated_at = sqlite3_column_int64(stmt, 7);
    v.push_back(std::move(r));
  }
  sqlite3_finalize(stmt);
  return v;
}

std::vector<KaringRecord> KaringDao::search_text(const std::string& term, int limit, int offset) {
  Db db(db_path_);
  std::vector<KaringRecord> v;
  if (!db.ok()) return v;
  const char* sql =
      "SELECT k.id, k.is_file, k.key, k.content, k.filename, k.mime, k.created_at, k.updated_at\n"
      "FROM karing k JOIN karing_fts f ON f.rowid = k.id\n"
      "WHERE k.is_active=1 AND k.is_file=0 AND karing_fts MATCH ?\n"
      "ORDER BY k.created_at DESC, k.id DESC LIMIT ? OFFSET ?;";
  sqlite3_stmt* stmt = nullptr;
  if (sqlite3_prepare_v2(db, sql, -1, &stmt, nullptr) != SQLITE_OK) return v;
  sqlite3_bind_text(stmt, 1, term.c_str(), -1, SQLITE_TRANSIENT);
  sqlite3_bind_int(stmt, 2, limit);
  sqlite3_bind_int(stmt, 3, offset);
  while (sqlite3_step(stmt) == SQLITE_ROW) {
    KaringRecord r{};
    r.id = sqlite3_column_int(stmt, 0);
    r.is_file = sqlite3_column_int(stmt, 1) != 0;
    if (const unsigned char* t = sqlite3_column_text(stmt, 2)) r.key = reinterpret_cast<const char*>(t);
    if (const unsigned char* t = sqlite3_column_text(stmt, 3)) r.content = reinterpret_cast<const char*>(t);
    if (const unsigned char* t = sqlite3_column_text(stmt, 4)) r.filename = reinterpret_cast<const char*>(t);
    if (const unsigned char* t = sqlite3_column_text(stmt, 5)) r.mime = reinterpret_cast<const char*>(t);
    r.created_at = sqlite3_column_int64(stmt, 6);
    if (sqlite3_column_type(stmt, 7) != SQLITE_NULL) r.updated_at = sqlite3_column_int64(stmt, 7);
    v.push_back(std::move(r));
  }
  sqlite3_finalize(stmt);
  return v;
}

bool KaringDao::try_search_fts(const std::string& fts_query, int limit, int offset, std::vector<KaringRecord>& v) {
  Db db(db_path_);
  if (!db.ok()) return false;
  const char* sql =
      "SELECT k.id, k.is_file, k.key, k.content, k.filename, k.mime, k.created_at, k.updated_at\n"
      "FROM karing k JOIN karing_fts f ON f.rowid = k.id\n"
      "WHERE k.is_active=1 AND k.is_file=0 AND karing_fts MATCH ?\n"
      "ORDER BY k.created_at DESC, k.id DESC LIMIT ? OFFSET ?;";
  sqlite3_stmt* stmt = nullptr;
  int rc = sqlite3_prepare_v2(db, sql, -1, &stmt, nullptr);
  if (rc != SQLITE_OK) return false;
  sqlite3_bind_text(stmt, 1, fts_query.c_str(), -1, SQLITE_TRANSIENT);
  sqlite3_bind_int(stmt, 2, limit);
  sqlite3_bind_int(stmt, 3, offset);
  while (sqlite3_step(stmt) == SQLITE_ROW) {
    KaringRecord r{};
    r.id = sqlite3_column_int(stmt, 0);
    r.is_file = sqlite3_column_int(stmt, 1) != 0;
    if (const unsigned char* t = sqlite3_column_text(stmt, 2)) r.key = reinterpret_cast<const char*>(t);
    if (const unsigned char* t = sqlite3_column_text(stmt, 3)) r.content = reinterpret_cast<const char*>(t);
    if (const unsigned char* t = sqlite3_column_text(stmt, 4)) r.filename = reinterpret_cast<const char*>(t);
    if (const unsigned char* t = sqlite3_column_text(stmt, 5)) r.mime = reinterpret_cast<const char*>(t);
    r.created_at = sqlite3_column_int64(stmt, 6);
    if (sqlite3_column_type(stmt, 7) != SQLITE_NULL) r.updated_at = sqlite3_column_int64(stmt, 7);
    v.push_back(std::move(r));
  }
  sqlite3_finalize(stmt);
  return true;
}

std::vector<KaringRecord> KaringDao::search_text_like(const std::string& needle, int limit, int offset) {
  Db db(db_path_);
  std::vector<KaringRecord> v;
  if (!db.ok()) return v;
  const char* sql =
      "SELECT id, is_file, key, content, filename, mime, created_at, updated_at\n"
      "FROM karing WHERE is_active=1 AND is_file=0 AND content LIKE ? ESCAPE '\\'\n"
      "ORDER BY created_at DESC, id DESC LIMIT ? OFFSET ?;";
  sqlite3_stmt* stmt = nullptr;
  if (sqlite3_prepare_v2(db, sql, -1, &stmt, nullptr) != SQLITE_OK) return v;
  // escape % and _
  std::string esc; esc.reserve(needle.size()*2+2);
  for (char c : needle) {
    if (c == '%' || c == '_' || c == '\\') esc.push_back('\\');
    esc.push_back(c);
  }
  std::string like = "%" + esc + "%";
  sqlite3_bind_text(stmt, 1, like.c_str(), -1, SQLITE_TRANSIENT);
  sqlite3_bind_int(stmt, 2, limit);
  sqlite3_bind_int(stmt, 3, offset);
  while (sqlite3_step(stmt) == SQLITE_ROW) {
    KaringRecord r{};
    r.id = sqlite3_column_int(stmt, 0);
    r.is_file = sqlite3_column_int(stmt, 1) != 0;
    if (const unsigned char* t = sqlite3_column_text(stmt, 2)) r.key = reinterpret_cast<const char*>(t);
    if (const unsigned char* t = sqlite3_column_text(stmt, 3)) r.content = reinterpret_cast<const char*>(t);
    if (const unsigned char* t = sqlite3_column_text(stmt, 4)) r.filename = reinterpret_cast<const char*>(t);
    if (const unsigned char* t = sqlite3_column_text(stmt, 5)) r.mime = reinterpret_cast<const char*>(t);
    r.created_at = sqlite3_column_int64(stmt, 6);
    if (sqlite3_column_type(stmt, 7) != SQLITE_NULL) r.updated_at = sqlite3_column_int64(stmt, 7);
    v.push_back(std::move(r));
  }
  sqlite3_finalize(stmt);
  return v;
}




std::vector<KaringRecord> KaringDao::list_filtered(int limit, int offset, const Filters& f) {
  Db db(db_path_);
  std::vector<KaringRecord> v;
  if (!db.ok()) return v;
  std::string sql = "SELECT id, is_file, key, content, filename, mime, created_at, updated_at FROM karing WHERE 1=1";
  if (!f.include_inactive) sql += " AND is_active=1";
  if (f.key) sql += " AND key = ?";
  if (f.is_file) sql += " AND is_file = ?";
  if (f.mime) sql += " AND mime = ?";
  if (f.filename) sql += " AND filename = ?";
  sql += f.order_desc ? " ORDER BY created_at DESC, id DESC" : " ORDER BY created_at ASC, id ASC";
  sql += " LIMIT ? OFFSET ?";
  sqlite3_stmt* stmt = nullptr;
  if (sqlite3_prepare_v2(db, sql.c_str(), -1, &stmt, nullptr) != SQLITE_OK) return v;
  int idx = 1;
  if (f.key) sqlite3_bind_text(stmt, idx++, f.key->c_str(), -1, SQLITE_TRANSIENT);
  if (f.is_file) sqlite3_bind_int(stmt, idx++, *f.is_file);
  if (f.mime) sqlite3_bind_text(stmt, idx++, f.mime->c_str(), -1, SQLITE_TRANSIENT);
  if (f.filename) sqlite3_bind_text(stmt, idx++, f.filename->c_str(), -1, SQLITE_TRANSIENT);
  sqlite3_bind_int(stmt, idx++, limit);
  sqlite3_bind_int(stmt, idx++, offset);
  while (sqlite3_step(stmt) == SQLITE_ROW) {
    KaringRecord r{};
    r.id = sqlite3_column_int(stmt, 0);
    r.is_file = sqlite3_column_int(stmt, 1) != 0;
    if (const unsigned char* t = sqlite3_column_text(stmt, 2)) r.key = reinterpret_cast<const char*>(t);
    if (const unsigned char* t = sqlite3_column_text(stmt, 3)) r.content = reinterpret_cast<const char*>(t);
    if (const unsigned char* t = sqlite3_column_text(stmt, 4)) r.filename = reinterpret_cast<const char*>(t);
    if (const unsigned char* t = sqlite3_column_text(stmt, 5)) r.mime = reinterpret_cast<const char*>(t);
    r.created_at = sqlite3_column_int64(stmt, 6);
    if (sqlite3_column_type(stmt, 7) != SQLITE_NULL) r.updated_at = sqlite3_column_int64(stmt, 7);
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
  if (f.key) sql += " AND key = ?";
  if (f.is_file) sql += " AND is_file = ?";
  if (f.mime) sql += " AND mime = ?";
  if (f.filename) sql += " AND filename = ?";
  sqlite3_stmt* stmt = nullptr;
  if (sqlite3_prepare_v2(db, sql.c_str(), -1, &stmt, nullptr) != SQLITE_OK) return 0;
  int idx = 1;
  if (f.key) sqlite3_bind_text(stmt, idx++, f.key->c_str(), -1, SQLITE_TRANSIENT);
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
      "WHERE k.is_active=1 AND k.is_file=0 AND karing_fts MATCH ?;";
  sqlite3_stmt* stmt = nullptr;
  if (sqlite3_prepare_v2(db, sql, -1, &stmt, nullptr) != SQLITE_OK) return false;
  sqlite3_bind_text(stmt, 1, fts_query.c_str(), -1, SQLITE_TRANSIENT);
  long long c = 0;
  if (sqlite3_step(stmt) == SQLITE_ROW) c = sqlite3_column_int64(stmt, 0);
  sqlite3_finalize(stmt);
  out = c;
  return true;
}

long long KaringDao::count_search_like(const std::string& needle) {
  Db db(db_path_);
  if (!db.ok()) return 0;
  const char* sql =
      "SELECT COUNT(1) FROM karing WHERE is_active=1 AND is_file=0 AND content LIKE ? ESCAPE '\\';";
  sqlite3_stmt* stmt = nullptr;
  if (sqlite3_prepare_v2(db, sql, -1, &stmt, nullptr) != SQLITE_OK) return 0;
  std::string esc; esc.reserve(needle.size()*2+2);
  for (char c : needle) { if (c=='%'||c=='_'||c=='\\') esc.push_back('\\'); esc.push_back(c); }
  std::string like = "%" + esc + "%";
  sqlite3_bind_text(stmt, 1, like.c_str(), -1, SQLITE_TRANSIENT);
  long long c = 0;
  if (sqlite3_step(stmt) == SQLITE_ROW) c = sqlite3_column_int64(stmt, 0);
  sqlite3_finalize(stmt);
  return c;
}

bool KaringDao::try_search_fts_after(const std::string& fts_query, int limit, long long cursor_ts, int cursor_id, std::vector<KaringRecord>& outv) {
  Db db(db_path_);
  if (!db.ok()) return false;
  const char* sql =
      "SELECT k.id, k.is_file, k.key, k.content, k.filename, k.mime, k.created_at, k.updated_at\n"
      "FROM karing k JOIN karing_fts f ON f.rowid = k.id\n"
      "WHERE k.is_active=1 AND k.is_file=0 AND karing_fts MATCH ? AND (k.created_at < ? OR (k.created_at = ? AND k.id < ?))\n"
      "ORDER BY k.created_at DESC, k.id DESC LIMIT ?;";
  sqlite3_stmt* stmt = nullptr;
  int rc = sqlite3_prepare_v2(db, sql, -1, &stmt, nullptr);
  if (rc != SQLITE_OK) return false;
  sqlite3_bind_text(stmt, 1, fts_query.c_str(), -1, SQLITE_TRANSIENT);
  sqlite3_bind_int64(stmt, 2, cursor_ts);
  sqlite3_bind_int64(stmt, 3, cursor_ts);
  sqlite3_bind_int(stmt, 4, cursor_id);
  sqlite3_bind_int(stmt, 5, limit);
  while (sqlite3_step(stmt) == SQLITE_ROW) {
    KaringRecord r{};
    r.id = sqlite3_column_int(stmt, 0);
    r.is_file = sqlite3_column_int(stmt, 1) != 0;
    if (const unsigned char* t = sqlite3_column_text(stmt, 2)) r.key = reinterpret_cast<const char*>(t);
    if (const unsigned char* t = sqlite3_column_text(stmt, 3)) r.content = reinterpret_cast<const char*>(t);
    if (const unsigned char* t = sqlite3_column_text(stmt, 4)) r.filename = reinterpret_cast<const char*>(t);
    if (const unsigned char* t = sqlite3_column_text(stmt, 5)) r.mime = reinterpret_cast<const char*>(t);
    r.created_at = sqlite3_column_int64(stmt, 6);
    if (sqlite3_column_type(stmt, 7) != SQLITE_NULL) r.updated_at = sqlite3_column_int64(stmt, 7);
    outv.push_back(std::move(r));
  }
  sqlite3_finalize(stmt);
  return true;
}

std::vector<KaringRecord> KaringDao::search_text_like_after(const std::string& needle, int limit, long long cursor_ts, int cursor_id) {
  Db db(db_path_);
  std::vector<KaringRecord> v;
  if (!db.ok()) return v;
  const char* sql =
      "SELECT id, is_file, key, content, filename, mime, created_at, updated_at\n"
      "FROM karing WHERE is_active=1 AND is_file=0 AND content LIKE ? ESCAPE '\\' AND (created_at < ? OR (created_at = ? AND id < ?))\n"
      "ORDER BY created_at DESC, id DESC LIMIT ?;";
  sqlite3_stmt* stmt = nullptr;
  if (sqlite3_prepare_v2(db, sql, -1, &stmt, nullptr) != SQLITE_OK) return v;
  std::string esc; esc.reserve(needle.size()*2+2);
  for (char c : needle) { if (c=='%'||c=='_'||c=='\\') esc.push_back('\\'); esc.push_back(c); }
  std::string like = "%" + esc + "%";
  sqlite3_bind_text(stmt, 1, like.c_str(), -1, SQLITE_TRANSIENT);
  sqlite3_bind_int64(stmt, 2, cursor_ts);
  sqlite3_bind_int64(stmt, 3, cursor_ts);
  sqlite3_bind_int(stmt, 4, cursor_id);
  sqlite3_bind_int(stmt, 5, limit);
  while (sqlite3_step(stmt) == SQLITE_ROW) {
    KaringRecord r{};
    r.id = sqlite3_column_int(stmt, 0);
    r.is_file = sqlite3_column_int(stmt, 1) != 0;
    if (const unsigned char* t = sqlite3_column_text(stmt, 2)) r.key = reinterpret_cast<const char*>(t);
    if (const unsigned char* t = sqlite3_column_text(stmt, 3)) r.content = reinterpret_cast<const char*>(t);
    if (const unsigned char* t = sqlite3_column_text(stmt, 4)) r.filename = reinterpret_cast<const char*>(t);
    if (const unsigned char* t = sqlite3_column_text(stmt, 5)) r.mime = reinterpret_cast<const char*>(t);
    r.created_at = sqlite3_column_int64(stmt, 6);
    if (sqlite3_column_type(stmt, 7) != SQLITE_NULL) r.updated_at = sqlite3_column_int64(stmt, 7);
    v.push_back(std::move(r));
  }
  sqlite3_finalize(stmt);
  return v;
}

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

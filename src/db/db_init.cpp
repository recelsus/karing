// SQLite schema init using raw C API; separate path ensuring.
#include <filesystem>
#include <fstream>
#include <sqlite3.h>
#include "schemas.h"

namespace fs = std::filesystem;
namespace karing::db {

static void ensure_parent_dir(const fs::path& p) {
  auto dir = p.parent_path();
  if (!dir.empty() && !fs::exists(dir)) {
    fs::create_directories(dir);
  }
}

std::string ensure_db_path(const std::string& db_path_str) {
  fs::path db_path = fs::path(db_path_str);
  ensure_parent_dir(db_path);
  if (!fs::exists(db_path)) {
    std::ofstream f(db_path);
    f.close();
  }
  return fs::weakly_canonical(db_path).string();
}

void init_sqlite_schema_file(const std::string& db_path_str) {
  sqlite3* db = nullptr;
  if (sqlite3_open_v2(db_path_str.c_str(), &db, SQLITE_OPEN_READWRITE | SQLITE_OPEN_CREATE, nullptr) != SQLITE_OK) {
    if (db) sqlite3_close(db);
    return;
  }
  char* errmsg = nullptr;
  auto exec = [&](const char* sql) {
    int rc = sqlite3_exec(db, sql, nullptr, nullptr, &errmsg);
    if (rc != SQLITE_OK && errmsg) { sqlite3_free(errmsg); errmsg = nullptr; }
  };
  exec("PRAGMA journal_mode = WAL;");
  exec("PRAGMA synchronous = NORMAL;");
  exec("PRAGMA foreign_keys = ON;");
  exec("BEGIN;");
  exec("CREATE TABLE IF NOT EXISTS config (\n  name TEXT PRIMARY KEY,\n  value TEXT NOT NULL\n);");
  exec("CREATE TABLE IF NOT EXISTS api_keys (\n  id INTEGER PRIMARY KEY,\n  key TEXT NOT NULL UNIQUE,\n  label TEXT,\n  enabled INTEGER NOT NULL DEFAULT 1,\n  created_at INTEGER NOT NULL,\n  last_used_at INTEGER,\n  last_ip TEXT\n);");
  exec("CREATE TABLE IF NOT EXISTS ip_allow (\n  id INTEGER PRIMARY KEY,\n  cidr TEXT NOT NULL UNIQUE,\n  enabled INTEGER NOT NULL DEFAULT 1,\n  created_at INTEGER NOT NULL\n);");
  exec("CREATE TABLE IF NOT EXISTS ip_deny (\n  id INTEGER PRIMARY KEY,\n  cidr TEXT NOT NULL UNIQUE,\n  enabled INTEGER NOT NULL DEFAULT 1,\n  created_at INTEGER NOT NULL\n);");
  exec("CREATE TABLE IF NOT EXISTS karing (\n  id INTEGER PRIMARY KEY,\n  key TEXT UNIQUE,\n  content TEXT,\n  syntax TEXT,\n  is_file INTEGER NOT NULL DEFAULT 0,\n  filename TEXT,\n  mime TEXT,\n  content_blob BLOB,\n  created_at INTEGER,\n  updated_at INTEGER,\n  client_ip TEXT,\n  api_key_id INTEGER,\n  revision INTEGER NOT NULL DEFAULT 0,\n  is_active INTEGER NOT NULL DEFAULT 0,\n  CHECK (is_active IN (0,1)),\n  CHECK (\n    is_active = 0 OR (\n      (is_file = 0 AND content IS NOT NULL AND content_blob IS NULL)\n      OR\n      (is_file = 1 AND content_blob IS NOT NULL AND content IS NULL AND filename IS NOT NULL AND mime IS NOT NULL)\n    )\n  ),\n  FOREIGN KEY (api_key_id) REFERENCES api_keys(id)\n    ON UPDATE CASCADE ON DELETE SET NULL\n);");
  exec("CREATE INDEX IF NOT EXISTS idx_karing_created_at_desc ON karing(created_at DESC, id DESC);");
  exec("CREATE INDEX IF NOT EXISTS idx_karing_key ON karing(key);");
  exec("CREATE INDEX IF NOT EXISTS idx_karing_is_file_created ON karing(is_file, created_at DESC, id DESC);");
  exec("CREATE INDEX IF NOT EXISTS idx_karing_filename ON karing(filename);");
  exec("CREATE INDEX IF NOT EXISTS idx_karing_mime ON karing(mime);");
  exec("CREATE TABLE IF NOT EXISTS overwrite_log (\n  id INTEGER PRIMARY KEY,\n  replaced_rowid INTEGER NOT NULL,\n  at INTEGER NOT NULL,\n  by_api_key_id INTEGER,\n  from_ip TEXT,\n  FOREIGN KEY (by_api_key_id) REFERENCES api_keys(id)\n    ON UPDATE CASCADE ON DELETE SET NULL\n);");
  exec("CREATE INDEX IF NOT EXISTS idx_overwrite_at ON overwrite_log(at DESC);");
  exec("COMMIT;");
  // optional FTS
  exec("CREATE VIRTUAL TABLE IF NOT EXISTS karing_fts USING fts5(content, content='karing', content_rowid='id');");
  exec("CREATE TRIGGER IF NOT EXISTS karing_ai\nAFTER INSERT ON karing\nWHEN NEW.is_active = 1 AND NEW.is_file = 0\nBEGIN\n  INSERT INTO karing_fts(rowid, content) VALUES (NEW.id, NEW.content);\nEND;\n");
  exec("CREATE TRIGGER IF NOT EXISTS karing_au\nAFTER UPDATE OF content, is_active, is_file ON karing\nBEGIN\n  INSERT INTO karing_fts(karing_fts, rowid, content) VALUES ('delete', OLD.id, OLD.content);\n  INSERT INTO karing_fts(rowid, content) SELECT NEW.id, NEW.content WHERE NEW.is_active = 1 AND NEW.is_file = 0;\nEND;\n");
  exec("CREATE TRIGGER IF NOT EXISTS karing_ad\nAFTER DELETE ON karing\nBEGIN\n  INSERT INTO karing_fts(karing_fts, rowid, content) VALUES ('delete', OLD.id, OLD.content);\nEND;\n");
  sqlite3_close(db);
}

}

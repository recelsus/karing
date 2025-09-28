#include "schemas.h"

namespace karing::db::schema {

void create_overwrite_log_table(const drogon::orm::DbClientPtr& client) {
  client->execSqlSync(
      "CREATE TABLE IF NOT EXISTS overwrite_log (\n"
      "  id             INTEGER PRIMARY KEY,\n"
      "  replaced_rowid INTEGER NOT NULL,\n"
      "  at             INTEGER NOT NULL,\n"
      "  by_api_key_id  INTEGER,\n"
      "  from_ip        TEXT,\n"
      "  FOREIGN KEY (by_api_key_id) REFERENCES api_keys(id)\n"
      "    ON UPDATE CASCADE ON DELETE SET NULL\n"
      ");");
  client->execSqlSync("CREATE INDEX IF NOT EXISTS idx_overwrite_at ON overwrite_log(at DESC);");

  // Add diff columns (ignore errors if already present)
  try { client->execSqlSync("ALTER TABLE overwrite_log ADD COLUMN action TEXT;"); } catch (...) {}
  try { client->execSqlSync("ALTER TABLE overwrite_log ADD COLUMN fields TEXT;"); } catch (...) {}
  try { client->execSqlSync("ALTER TABLE overwrite_log ADD COLUMN text_excerpt_before TEXT;"); } catch (...) {}
  try { client->execSqlSync("ALTER TABLE overwrite_log ADD COLUMN text_excerpt_after TEXT;"); } catch (...) {}
  try { client->execSqlSync("ALTER TABLE overwrite_log ADD COLUMN text_hash_before TEXT;"); } catch (...) {}
  try { client->execSqlSync("ALTER TABLE overwrite_log ADD COLUMN text_hash_after TEXT;"); } catch (...) {}
  try { client->execSqlSync("ALTER TABLE overwrite_log ADD COLUMN text_len_before INTEGER;"); } catch (...) {}
  try { client->execSqlSync("ALTER TABLE overwrite_log ADD COLUMN text_len_after INTEGER;"); } catch (...) {}
  try { client->execSqlSync("ALTER TABLE overwrite_log ADD COLUMN file_hash_before TEXT;"); } catch (...) {}
  try { client->execSqlSync("ALTER TABLE overwrite_log ADD COLUMN file_hash_after TEXT;"); } catch (...) {}
  try { client->execSqlSync("ALTER TABLE overwrite_log ADD COLUMN file_len_before INTEGER;"); } catch (...) {}
  try { client->execSqlSync("ALTER TABLE overwrite_log ADD COLUMN file_len_after INTEGER;"); } catch (...) {}
  try { client->execSqlSync("ALTER TABLE overwrite_log ADD COLUMN mime_before TEXT;"); } catch (...) {}
  try { client->execSqlSync("ALTER TABLE overwrite_log ADD COLUMN mime_after TEXT;"); } catch (...) {}
  try { client->execSqlSync("ALTER TABLE overwrite_log ADD COLUMN filename_before TEXT;"); } catch (...) {}
  try { client->execSqlSync("ALTER TABLE overwrite_log ADD COLUMN filename_after TEXT;"); } catch (...) {}
  try { client->execSqlSync("ALTER TABLE overwrite_log ADD COLUMN content_before TEXT;"); } catch (...) {}
  try { client->execSqlSync("ALTER TABLE overwrite_log ADD COLUMN text_patch TEXT;"); } catch (...) {}
}

}

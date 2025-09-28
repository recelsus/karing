#include "schemas.h"

namespace karing::db::schema {

void create_karing_table_and_indexes(const drogon::orm::DbClientPtr& client) {
  client->execSqlSync(
      "CREATE TABLE IF NOT EXISTS karing (\n"
      "  id           INTEGER PRIMARY KEY,\n"
      "  content      TEXT,\n"
      "  is_file      INTEGER NOT NULL DEFAULT 0,\n"
      "  filename     TEXT,\n"
      "  mime         TEXT,\n"
      "  content_blob BLOB,\n"
      "  created_at   INTEGER,\n"
      "  updated_at   INTEGER,\n"
      "  revision     INTEGER NOT NULL DEFAULT 0,\n"
      "  is_active    INTEGER NOT NULL DEFAULT 0,\n"
      "  CHECK (is_active IN (0,1)),\n"
      "  CHECK (\n"
      "    is_active = 0 OR (\n"
      "      (is_file = 0 AND content IS NOT NULL AND content_blob IS NULL)\n"
      "      OR\n"
      "      (is_file = 1 AND content_blob IS NOT NULL AND content IS NULL AND filename IS NOT NULL AND mime IS NOT NULL)\n"
      "    )\n"
      "  )\n"
      ");");

  client->execSqlSync("CREATE INDEX IF NOT EXISTS idx_karing_created_at_desc ON karing(created_at DESC, id DESC);");
  client->execSqlSync("CREATE INDEX IF NOT EXISTS idx_karing_is_file_created ON karing(is_file, created_at DESC, id DESC);");
  client->execSqlSync("CREATE INDEX IF NOT EXISTS idx_karing_filename        ON karing(filename);");
  client->execSqlSync("CREATE INDEX IF NOT EXISTS idx_karing_mime            ON karing(mime);");
}

}

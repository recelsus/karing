#include "schemas.h"

namespace karing::db::schema {

void create_auth_tables(const drogon::orm::DbClientPtr& client) {
  client->execSqlSync(
      "CREATE TABLE IF NOT EXISTS api_keys (\n"
      "  id           INTEGER PRIMARY KEY,\n"
      "  key          TEXT NOT NULL UNIQUE,\n"
      "  label        TEXT,\n"
      "  enabled      INTEGER NOT NULL DEFAULT 1,\n"
      "  role         TEXT NOT NULL DEFAULT 'user',\n"
      "  created_at   INTEGER NOT NULL,\n"
      "  last_used_at INTEGER,\n"
      "  last_ip      TEXT\n"
      ");");
  client->execSqlSync(
      "CREATE TABLE IF NOT EXISTS ip_rules (\n"
      "  id           INTEGER PRIMARY KEY,\n"
      "  pattern      TEXT NOT NULL,\n"
      "  permission   TEXT NOT NULL CHECK (permission IN ('allow','deny')),\n"
      "  enabled      INTEGER NOT NULL DEFAULT 1,\n"
      "  created_at   INTEGER NOT NULL,\n"
      "  UNIQUE(pattern, permission)\n"
      ");");
}

}

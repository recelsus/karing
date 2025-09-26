#include "schemas.h"

namespace karing::db::schema {

void create_auth_tables(const drogon::orm::DbClientPtr& client) {
  client->execSqlSync(
      "CREATE TABLE IF NOT EXISTS api_keys (\n"
      "  id           INTEGER PRIMARY KEY,\n"
      "  key          TEXT NOT NULL UNIQUE,\n"
      "  label        TEXT,\n"
      "  enabled      INTEGER NOT NULL DEFAULT 1,\n"
      "  role         TEXT NOT NULL DEFAULT 'write',\n"
      "  created_at   INTEGER NOT NULL,\n"
      "  last_used_at INTEGER,\n"
      "  last_ip      TEXT\n"
      ");");
  // migrate
  try { client->execSqlSync("ALTER TABLE api_keys ADD COLUMN role TEXT NOT NULL DEFAULT 'write';"); } catch (...) {}

  client->execSqlSync(
      "CREATE TABLE IF NOT EXISTS ip_allow (\n"
      "  id           INTEGER PRIMARY KEY,\n"
      "  cidr         TEXT NOT NULL UNIQUE,\n"
      "  enabled      INTEGER NOT NULL DEFAULT 1,\n"
      "  created_at   INTEGER NOT NULL\n"
      ");");

  client->execSqlSync(
      "CREATE TABLE IF NOT EXISTS ip_deny (\n"
      "  id           INTEGER PRIMARY KEY,\n"
      "  cidr         TEXT NOT NULL UNIQUE,\n"
      "  enabled      INTEGER NOT NULL DEFAULT 1,\n"
      "  created_at   INTEGER NOT NULL\n"
      ");");
}

}

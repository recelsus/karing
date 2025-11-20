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
  // migrate existing role column / values
  try { client->execSqlSync("ALTER TABLE api_keys ADD COLUMN role TEXT NOT NULL DEFAULT 'user';"); } catch (...) {}
  try { client->execSqlSync("UPDATE api_keys SET role='user' WHERE role IS NULL OR role IN ('read','write');"); } catch (...) {}

  client->execSqlSync(
      "CREATE TABLE IF NOT EXISTS ip_rules (\n"
      "  id           INTEGER PRIMARY KEY,\n"
      "  pattern      TEXT NOT NULL,\n"
      "  permission   TEXT NOT NULL CHECK (permission IN ('allow','deny')),\n"
      "  enabled      INTEGER NOT NULL DEFAULT 1,\n"
      "  created_at   INTEGER NOT NULL,\n"
      "  UNIQUE(pattern, permission)\n"
      ");");
  try { client->execSqlSync(
            "INSERT OR IGNORE INTO ip_rules(pattern, permission, enabled, created_at)\n"
            " SELECT cidr, 'allow', enabled, created_at FROM ip_allow;" ); }
  catch (...) {}
  try { client->execSqlSync(
            "INSERT OR IGNORE INTO ip_rules(pattern, permission, enabled, created_at)\n"
            " SELECT cidr, 'deny', enabled, created_at FROM ip_deny;" ); }
  catch (...) {}
  try { client->execSqlSync("DROP TABLE IF EXISTS ip_allow;"); } catch (...) {}
  try { client->execSqlSync("DROP TABLE IF EXISTS ip_deny;"); } catch (...) {}
}

}

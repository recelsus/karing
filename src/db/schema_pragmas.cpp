#include "schemas.h"

namespace karing::db::schema {

void apply_pragmas(const drogon::orm::DbClientPtr& client) {
  client->execSqlSync("PRAGMA journal_mode = WAL;");
  client->execSqlSync("PRAGMA synchronous = NORMAL;");
  client->execSqlSync("PRAGMA foreign_keys = ON;");
}

}


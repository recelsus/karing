#include "schemas.h"

namespace karing::db::schema {

void create_config_table(const drogon::orm::DbClientPtr& client) {
  client->execSqlSync(
      "CREATE TABLE IF NOT EXISTS config (\n"
      "  name  TEXT PRIMARY KEY,\n"
      "  value TEXT NOT NULL\n"
      ");");
}

}


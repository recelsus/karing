#include "db_introspection.h"
#include <sqlite3.h>

namespace karing::db::inspect {

std::vector<std::pair<std::string, std::string>> list_tables_with_sql(const std::string& db_path) {
  sqlite3* db = nullptr;
  std::vector<std::pair<std::string, std::string>> out;
  if (sqlite3_open_v2(db_path.c_str(), &db, SQLITE_OPEN_READONLY, nullptr) != SQLITE_OK) {
    if (db) sqlite3_close(db);
    return out;
  }
  const char* sql = "SELECT name, sql FROM sqlite_master WHERE type='table' ORDER BY name;";
  sqlite3_stmt* stmt = nullptr;
  if (sqlite3_prepare_v2(db, sql, -1, &stmt, nullptr) == SQLITE_OK) {
    while (sqlite3_step(stmt) == SQLITE_ROW) {
      const unsigned char* n = sqlite3_column_text(stmt, 0);
      const unsigned char* s = sqlite3_column_text(stmt, 1);
      out.emplace_back(n ? reinterpret_cast<const char*>(n) : "",
                       s ? reinterpret_cast<const char*>(s) : "");
    }
  }
  if (stmt) sqlite3_finalize(stmt);
  sqlite3_close(db);
  return out;
}

}

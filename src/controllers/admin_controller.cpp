#include "admin_controller.h"
#include <drogon/drogon.h>
#include <sqlite3.h>
#include <string>
#include "utils/options.h"

namespace karing::controllers {

void admin_controller::list_auth(const drogon::HttpRequestPtr& req,
                                 std::function<void(const drogon::HttpResponsePtr&)>&& cb) {
  auto& options_state = karing::options::runtime_options::instance();
  // Open DB (read-only)
  sqlite3* db = nullptr;
  if (sqlite3_open_v2(options_state.db_path().c_str(), &db, SQLITE_OPEN_READONLY, nullptr) != SQLITE_OK) {
    auto resp = drogon::HttpResponse::newHttpResponse();
    resp->setStatusCode(drogon::k500InternalServerError);
    cb(resp);
    if (db) sqlite3_close(db);
    return;
  }
  Json::Value out(Json::objectValue);
  // API keys list
  {
    Json::Value arr(Json::arrayValue);
    const char* sql = "SELECT id, key, label, enabled, role, created_at, last_used_at, last_ip FROM api_keys ORDER BY id;";
    sqlite3_stmt* st = nullptr;
    if (sqlite3_prepare_v2(db, sql, -1, &st, nullptr) == SQLITE_OK) {
      while (sqlite3_step(st) == SQLITE_ROW) {
        Json::Value row(Json::objectValue);
        row["id"] = sqlite3_column_int(st, 0);
        if (const unsigned char* t = sqlite3_column_text(st, 1)) row["key"] = reinterpret_cast<const char*>(t);
        if (const unsigned char* t = sqlite3_column_text(st, 2)) row["label"] = reinterpret_cast<const char*>(t); else row["label"] = "";
        row["enabled"] = sqlite3_column_int(st, 3) != 0;
        if (const unsigned char* t = sqlite3_column_text(st, 4)) row["role"] = reinterpret_cast<const char*>(t); else row["role"] = "write";
        row["created_at"] = (Json::Int64)sqlite3_column_int64(st, 5);
        if (sqlite3_column_type(st,6) != SQLITE_NULL) row["last_used_at"] = (Json::Int64)sqlite3_column_int64(st, 6);
        if (const unsigned char* t = sqlite3_column_text(st, 7)) row["last_ip"] = reinterpret_cast<const char*>(t);
        arr.append(row);
      }
    }
    if (st) sqlite3_finalize(st);
    out["api_keys"] = std::move(arr);
  }
  // IP allow
  {
    Json::Value arr(Json::arrayValue);
    const char* sql = "SELECT id, pattern, permission, enabled, created_at FROM ip_rules ORDER BY id;";
    sqlite3_stmt* st = nullptr;
    if (sqlite3_prepare_v2(db, sql, -1, &st, nullptr) == SQLITE_OK) {
      while (sqlite3_step(st) == SQLITE_ROW) {
        Json::Value row(Json::objectValue);
        row["id"] = sqlite3_column_int(st, 0);
        if (const unsigned char* t = sqlite3_column_text(st, 1)) row["pattern"] = reinterpret_cast<const char*>(t);
        if (const unsigned char* t = sqlite3_column_text(st, 2)) row["permission"] = reinterpret_cast<const char*>(t);
        row["enabled"] = sqlite3_column_int(st, 3) != 0;
        row["created_at"] = (Json::Int64)sqlite3_column_int64(st, 4);
        arr.append(row);
      }
    }
    if (st) sqlite3_finalize(st);
    out["ip_rules"] = std::move(arr);
  }

  sqlite3_close(db);
  auto resp = drogon::HttpResponse::newHttpJsonResponse(out);
  resp->setStatusCode(drogon::k200OK);
  cb(resp);
}

}

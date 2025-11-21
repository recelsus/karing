#include "admin_controller.h"
#include <drogon/drogon.h>
#include <sqlite3.h>
#include <string>
#include <optional>
#include <memory>
#include <algorithm>
#include <cctype>
#include <sstream>
#include <openssl/rand.h>
#include "utils/options.h"
#include "utils/json_response.h"

namespace karing::controllers {

namespace {

std::string to_lower_copy(std::string s) {
  std::transform(s.begin(), s.end(), s.begin(), [](unsigned char c) { return static_cast<char>(std::tolower(c)); });
  return s;
}

std::optional<std::string> generate_secret_hex() {
  unsigned char buffer[24];
  if (RAND_bytes(buffer, sizeof(buffer)) != 1) return std::nullopt;
  static const char* hex = "0123456789abcdef";
  std::string secret(sizeof(buffer) * 2, '0');
  for (size_t i = 0; i < sizeof(buffer); ++i) {
    secret[i * 2] = hex[(buffer[i] >> 4) & 0xF];
    secret[i * 2 + 1] = hex[buffer[i] & 0xF];
  }
  return secret;
}

bool normalize_ip_pattern(const std::string& input, std::string& output) {
  auto slash = input.find('/');
  std::string ip_part = input.substr(0, slash);
  int prefix_bits = -1;
  if (slash != std::string::npos) {
    try {
      prefix_bits = std::stoi(input.substr(slash + 1));
    } catch (...) {
      return false;
    }
    if (prefix_bits < 0 || prefix_bits > 32) return false;
  }
  int octets[4] = {0, 0, 0, 0};
  char dummy;
  std::istringstream stream(ip_part);
  if (!(stream >> octets[0] >> dummy >> octets[1] >> dummy >> octets[2] >> dummy >> octets[3])) return false;
  for (int& octet : octets) {
    if (octet < 0 || octet > 255) return false;
  }
  if (prefix_bits < 0) {
    std::ostringstream oss;
    oss << octets[0] << '.' << octets[1] << '.' << octets[2] << '.' << octets[3];
    output = oss.str();
    return true;
  }
  uint32_t numeric = (static_cast<uint32_t>(octets[0]) << 24) |
                     (static_cast<uint32_t>(octets[1]) << 16) |
                     (static_cast<uint32_t>(octets[2]) << 8) |
                     (static_cast<uint32_t>(octets[3]));
  uint32_t mask = (prefix_bits == 0) ? 0u : 0xFFFFFFFFu << (32 - prefix_bits);
  uint32_t network = numeric & mask;
  std::ostringstream oss;
  oss << ((network >> 24) & 0xFF) << '.'
      << ((network >> 16) & 0xFF) << '.'
      << ((network >> 8) & 0xFF) << '.'
      << (network & 0xFF) << '/' << prefix_bits;
  output = oss.str();
  return true;
}

std::optional<int> json_get_int(const Json::Value& json, const std::string& key) {
  if (json.isMember(key)) {
    if (json[key].isInt()) return json[key].asInt();
    if (json[key].isString()) {
      try {
        return std::stoi(json[key].asString());
      } catch (...) {
        return std::nullopt;
      }
    }
  }
  return std::nullopt;
}

}  // namespace

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

void admin_controller::manage_auth(const drogon::HttpRequestPtr& req,
                                   std::function<void(const drogon::HttpResponsePtr&)>&& cb) {
  auto json = req->getJsonObject();
  if (!json || !(*json)["action"].isString()) {
    return cb(karing::http::error(drogon::k400BadRequest, "E_ACTION", "Action required"));
  }
  std::string action = to_lower_copy((*json)["action"].asString());
  sqlite3* raw_db = nullptr;
  auto& options_state = karing::options::runtime_options::instance();
  if (sqlite3_open_v2(options_state.db_path().c_str(), &raw_db, SQLITE_OPEN_READWRITE, nullptr) != SQLITE_OK) {
    if (raw_db) sqlite3_close(raw_db);
    return cb(karing::http::error(drogon::k500InternalServerError, "E_DB", "Database open failed"));
  }
  std::unique_ptr<sqlite3, decltype(&sqlite3_close)> db(raw_db, sqlite3_close);

  auto respond_ok = [&](const Json::Value& data) {
    cb(karing::http::ok(data));
  };

  auto respond_msg = [&](const std::string& message) {
    Json::Value data;
    data["success"] = true;
    data["message"] = message;
    respond_ok(data);
  };

  if (action == "create_api_key") {
    std::string role = json->isMember("role") ? to_lower_copy((*json)["role"].asString()) : "user";
    if (role != "user" && role != "admin") {
      return cb(karing::http::error(drogon::k400BadRequest, "E_ROLE", "Role must be user or admin"));
    }
    bool disabled = json->isMember("disabled") ? (*json)["disabled"].asBool() : false;
    std::string label = json->isMember("label") ? (*json)["label"].asString() : std::string();
    auto secret = generate_secret_hex();
    if (!secret) {
      return cb(karing::http::error(drogon::k500InternalServerError, "E_RANDOM", "Failed to generate API key"));
    }
    sqlite3_stmt* stmt = nullptr;
    const char* sql = "INSERT INTO api_keys(key,label,enabled,role,created_at) VALUES(?,?,?,?,strftime('%s','now'));";
    if (sqlite3_prepare_v2(db.get(), sql, -1, &stmt, nullptr) != SQLITE_OK) {
      if (stmt) sqlite3_finalize(stmt);
      return cb(karing::http::error(drogon::k500InternalServerError, "E_DB", "Prepare failed"));
    }
    sqlite3_bind_text(stmt, 1, secret->c_str(), -1, SQLITE_TRANSIENT);
    if (!label.empty()) sqlite3_bind_text(stmt, 2, label.c_str(), -1, SQLITE_TRANSIENT);
    else sqlite3_bind_null(stmt, 2);
    sqlite3_bind_int(stmt, 3, disabled ? 0 : 1);
    sqlite3_bind_text(stmt, 4, role.c_str(), -1, SQLITE_TRANSIENT);
    sqlite3_step(stmt);
    sqlite3_finalize(stmt);
    Json::Value data;
    data["id"] = static_cast<Json::Int64>(sqlite3_last_insert_rowid(db.get()));
    data["secret"] = *secret;
    data["role"] = role;
    data["enabled"] = !disabled;
    data["label"] = label;
    return respond_ok(data);
  }

  if (action == "set_api_key_role") {
    auto id = json_get_int(*json, "id");
    if (!id) return cb(karing::http::error(drogon::k400BadRequest, "E_ID", "id required"));
    if (!json->isMember("role") || !(*json)["role"].isString()) {
      return cb(karing::http::error(drogon::k400BadRequest, "E_ROLE", "role required"));
    }
    std::string role = to_lower_copy((*json)["role"].asString());
    if (role != "user" && role != "admin") {
      return cb(karing::http::error(drogon::k400BadRequest, "E_ROLE", "Role must be user or admin"));
    }
    sqlite3_stmt* stmt = nullptr;
    if (sqlite3_prepare_v2(db.get(), "UPDATE api_keys SET role=? WHERE id=?;", -1, &stmt, nullptr) == SQLITE_OK) {
      sqlite3_bind_text(stmt, 1, role.c_str(), -1, SQLITE_TRANSIENT);
      sqlite3_bind_int(stmt, 2, *id);
      sqlite3_step(stmt);
    }
    if (stmt) sqlite3_finalize(stmt);
    return respond_msg("role updated");
  }

  if (action == "set_api_key_label") {
    auto id = json_get_int(*json, "id");
    if (!id) return cb(karing::http::error(drogon::k400BadRequest, "E_ID", "id required"));
    if (!json->isMember("label")) {
      return cb(karing::http::error(drogon::k400BadRequest, "E_LABEL", "label required"));
    }
    std::string label = (*json)["label"].asString();
    sqlite3_stmt* stmt = nullptr;
    if (sqlite3_prepare_v2(db.get(), "UPDATE api_keys SET label=? WHERE id=?;", -1, &stmt, nullptr) == SQLITE_OK) {
      if (!label.empty()) sqlite3_bind_text(stmt, 1, label.c_str(), -1, SQLITE_TRANSIENT);
      else sqlite3_bind_null(stmt, 1);
      sqlite3_bind_int(stmt, 2, *id);
      sqlite3_step(stmt);
    }
    if (stmt) sqlite3_finalize(stmt);
    return respond_msg("label updated");
  }

  if (action == "set_api_key_enabled" || action == "enable_api_key" || action == "disable_api_key") {
    auto id = json_get_int(*json, "id");
    if (!id) return cb(karing::http::error(drogon::k400BadRequest, "E_ID", "id required"));
    bool enabled = true;
    if (action == "set_api_key_enabled") {
      if (!json->isMember("enabled")) {
        return cb(karing::http::error(drogon::k400BadRequest, "E_ENABLED", "enabled required"));
      }
      enabled = (*json)["enabled"].asBool();
    } else if (action == "disable_api_key") {
      enabled = false;
    } else {
      enabled = true;
    }
    sqlite3_stmt* stmt = nullptr;
    if (sqlite3_prepare_v2(db.get(), "UPDATE api_keys SET enabled=? WHERE id=?;", -1, &stmt, nullptr) == SQLITE_OK) {
      sqlite3_bind_int(stmt, 1, enabled ? 1 : 0);
      sqlite3_bind_int(stmt, 2, *id);
      sqlite3_step(stmt);
    }
    if (stmt) sqlite3_finalize(stmt);
    return respond_msg(enabled ? "enabled" : "disabled");
  }

  if (action == "delete_api_key") {
    auto id = json_get_int(*json, "id");
    if (!id) return cb(karing::http::error(drogon::k400BadRequest, "E_ID", "id required"));
    bool hard = json->isMember("hard") ? (*json)["hard"].asBool() : false;
    sqlite3_stmt* stmt = nullptr;
    std::string sql = hard ? "DELETE FROM api_keys WHERE id=?;" : "UPDATE api_keys SET enabled=0 WHERE id=?;";
    if (sqlite3_prepare_v2(db.get(), sql.c_str(), -1, &stmt, nullptr) == SQLITE_OK) {
      sqlite3_bind_int(stmt, 1, *id);
      sqlite3_step(stmt);
    }
    if (stmt) sqlite3_finalize(stmt);
    return respond_msg(hard ? "deleted" : "disabled");
  }

  if (action == "add_ip_rule") {
    if (!json->isMember("pattern") || !json->isMember("permission")) {
      return cb(karing::http::error(drogon::k400BadRequest, "E_IP", "pattern and permission required"));
    }
    std::string normalized;
    if (!normalize_ip_pattern((*json)["pattern"].asString(), normalized)) {
      return cb(karing::http::error(drogon::k400BadRequest, "E_IP", "Invalid IPv4 or CIDR"));
    }
    std::string permission = to_lower_copy((*json)["permission"].asString());
    if (permission != "allow" && permission != "deny") {
      return cb(karing::http::error(drogon::k400BadRequest, "E_IP", "Permission must be allow or deny"));
    }
    bool enabled = json->isMember("enabled") ? (*json)["enabled"].asBool() : true;
    sqlite3_stmt* stmt = nullptr;
    const char* sql =
        "INSERT INTO ip_rules(pattern, permission, enabled, created_at) VALUES(?,?,?,strftime('%s','now'))\n"
        "ON CONFLICT(pattern, permission) DO UPDATE SET enabled=excluded.enabled;";
    if (sqlite3_prepare_v2(db.get(), sql, -1, &stmt, nullptr) == SQLITE_OK) {
      sqlite3_bind_text(stmt, 1, normalized.c_str(), -1, SQLITE_TRANSIENT);
      sqlite3_bind_text(stmt, 2, permission.c_str(), -1, SQLITE_TRANSIENT);
      sqlite3_bind_int(stmt, 3, enabled ? 1 : 0);
      sqlite3_step(stmt);
    }
    if (stmt) sqlite3_finalize(stmt);
    sqlite3_stmt* q = nullptr;
    Json::Value data;
    if (sqlite3_prepare_v2(db.get(), "SELECT id FROM ip_rules WHERE pattern=? AND permission=?;", -1, &q, nullptr) == SQLITE_OK) {
      sqlite3_bind_text(q, 1, normalized.c_str(), -1, SQLITE_TRANSIENT);
      sqlite3_bind_text(q, 2, permission.c_str(), -1, SQLITE_TRANSIENT);
      if (sqlite3_step(q) == SQLITE_ROW) {
        data["id"] = sqlite3_column_int(q, 0);
      }
    }
    if (q) sqlite3_finalize(q);
    data["pattern"] = normalized;
    data["permission"] = permission;
    data["enabled"] = enabled;
    return respond_ok(data);
  }

  if (action == "update_ip_rule") {
    auto id = json_get_int(*json, "id");
    if (!id) return cb(karing::http::error(drogon::k400BadRequest, "E_ID", "id required"));
    std::vector<std::string> sets;
    std::vector<std::string> values;
    std::vector<std::optional<std::string>> str_binds;
    std::optional<bool> enabled;
    if (json->isMember("pattern")) {
      std::string normalized;
      if (!normalize_ip_pattern((*json)["pattern"].asString(), normalized)) {
        return cb(karing::http::error(drogon::k400BadRequest, "E_IP", "Invalid IPv4 or CIDR"));
      }
      sets.emplace_back("pattern=?");
      values.push_back(normalized);
    }
    if (json->isMember("permission")) {
      std::string permission = to_lower_copy((*json)["permission"].asString());
      if (permission != "allow" && permission != "deny") {
        return cb(karing::http::error(drogon::k400BadRequest, "E_IP", "Permission must be allow or deny"));
      }
      sets.emplace_back("permission=?");
      values.push_back(permission);
    }
    if (json->isMember("enabled")) {
      enabled = (*json)["enabled"].asBool();
      sets.emplace_back("enabled=?");
    }
    if (sets.empty()) {
      return cb(karing::http::error(drogon::k400BadRequest, "E_UPDATE", "No fields to update"));
    }
    std::ostringstream oss;
    oss << "UPDATE ip_rules SET ";
    for (size_t i = 0; i < sets.size(); ++i) {
      if (i) oss << ",";
      oss << sets[i];
    }
    oss << " WHERE id=?;";
    sqlite3_stmt* stmt = nullptr;
    if (sqlite3_prepare_v2(db.get(), oss.str().c_str(), -1, &stmt, nullptr) == SQLITE_OK) {
      int idx = 1;
      for (const auto& v : values) {
        sqlite3_bind_text(stmt, idx++, v.c_str(), -1, SQLITE_TRANSIENT);
      }
      if (enabled.has_value()) {
        sqlite3_bind_int(stmt, idx++, *enabled ? 1 : 0);
      }
      sqlite3_bind_int(stmt, idx, *id);
      sqlite3_step(stmt);
    }
    if (stmt) sqlite3_finalize(stmt);
    return respond_msg("ip rule updated");
  }

  if (action == "delete_ip_rule") {
    auto id = json_get_int(*json, "id");
    if (!id) return cb(karing::http::error(drogon::k400BadRequest, "E_ID", "id required"));
    sqlite3_stmt* stmt = nullptr;
    if (sqlite3_prepare_v2(db.get(), "DELETE FROM ip_rules WHERE id=?;", -1, &stmt, nullptr) == SQLITE_OK) {
      sqlite3_bind_int(stmt, 1, *id);
      sqlite3_step(stmt);
    }
    if (stmt) sqlite3_finalize(stmt);
    return respond_msg("ip rule deleted");
  }

  return cb(karing::http::error(drogon::k400BadRequest, "E_ACTION", "Unsupported action"));
}

}

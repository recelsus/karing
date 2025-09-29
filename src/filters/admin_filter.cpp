#include "admin_filter.h"
#include <drogon/drogon.h>
#include <sqlite3.h>
#include <string>
#include <optional>
#include "utils/options.h"
#include "utils/json_response.h"

using drogon::HttpRequestPtr;
using drogon::HttpStatusCode;

namespace karing::filters {

// Extract API key from header or query
static std::optional<std::string> extract_key(const HttpRequestPtr& req) {
  // Prefer header
  auto k = req->getHeader("x-api-key");
  if (!k.empty()) return k;
  // Fallback to query parameter
  auto p = req->getParameter("api_key");
  if (!p.empty()) return p;
  return std::nullopt;
}

void admin_filter::doFilter(const HttpRequestPtr& req,
                            drogon::FilterCallback&& fcb,
                            drogon::FilterChainCallback&& fccb) {
  // If no key is provided, assume auth_filter already permitted by IP allow-list or zero-key mode.
  // In that case, admin_filter lets it pass.
  auto key = extract_key(req);
  if (!key) return fccb();

  // Validate key has admin role
  sqlite3* db = nullptr;
  if (sqlite3_open_v2(karing::options::db_path().c_str(), &db, SQLITE_OPEN_READONLY, nullptr) != SQLITE_OK) {
    auto resp = karing::http::error(HttpStatusCode::k500InternalServerError, "E_DB", "Database open failed");
    fcb(resp);
    if (db) sqlite3_close(db);
    return;
  }
  bool ok = false;
  sqlite3_stmt* st = nullptr;
  if (sqlite3_prepare_v2(db, "SELECT 1 FROM api_keys WHERE key=? AND enabled=1 AND role='admin';", -1, &st, nullptr) == SQLITE_OK) {
    sqlite3_bind_text(st, 1, key->c_str(), -1, SQLITE_TRANSIENT);
    if (sqlite3_step(st) == SQLITE_ROW) ok = true;
  }
  if (st) sqlite3_finalize(st);
  sqlite3_close(db);
  if (!ok) {
    auto resp = karing::http::error(HttpStatusCode::k403Forbidden, "E_FORBIDDEN", "Insufficient role (admin required)");
    fcb(resp);
    return;
  }
  return fccb();
}

}

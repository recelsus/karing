#include "auth_filter.h"
#include <drogon/drogon.h>
#include <sqlite3.h>
#include <string>
#include <vector>
#include <optional>
#include "utils/options.h"
#include "utils/json_response.h"

using drogon::HttpRequestPtr;
using drogon::HttpStatusCode;

namespace karing::filters {

static std::vector<std::string> load_cidrs(sqlite3* db, const char* table) {
  std::vector<std::string> v;
  std::string sqlStr = std::string("SELECT cidr FROM ") + table + " WHERE enabled=1;";
  const char* sql = sqlStr.c_str();
  sqlite3_stmt* stmt = nullptr;
  if (sqlite3_prepare_v2(db, sql, -1, &stmt, nullptr) == SQLITE_OK) {
    while (sqlite3_step(stmt) == SQLITE_ROW) {
      if (const unsigned char* t = sqlite3_column_text(stmt, 0)) v.emplace_back(reinterpret_cast<const char*>(t));
    }
  }
  if (stmt) sqlite3_finalize(stmt);
  return v;
}

static bool parse_ipv4(const std::string& ip, uint32_t& out) {
  unsigned a,b,c,d; char ch;
  if (sscanf(ip.c_str(), "%u.%u.%u.%u%c", &a,&b,&c,&d,&ch) == 4 && a<256 && b<256 && c<256 && d<256) {
    out = (a<<24)|(b<<16)|(c<<8)|d; return true;
  }
  return false;
}

static bool cidr_match(const std::string& cidr, const std::string& ip) {
  auto pos = cidr.find('/');
  if (pos == std::string::npos) return cidr == ip;
  std::string net = cidr.substr(0,pos); int bits = std::stoi(cidr.substr(pos+1));
  uint32_t nip, tip; if (!parse_ipv4(net, nip) || !parse_ipv4(ip, tip)) return false;
  if (bits <=0) return true; if (bits >=32) return tip==nip;
  uint32_t mask = bits==0?0: htonl(~((1u<<(32-bits))-1));
  return (tip & mask) == (nip & mask);
}

static bool ip_in_list(const std::vector<std::string>& cidrs, const std::string& ip) {
  for (const auto& c : cidrs) if (cidr_match(c, ip)) return true; return false;
}

static std::optional<std::string> get_api_key(const HttpRequestPtr& req) {
  auto k = req->getHeader("x-api-key");
  if (!k.empty()) return k;
  auto p = req->getParameter("api_key");
  if (!p.empty()) return p;
  return std::nullopt;
}

void auth_filter::doFilter(const HttpRequestPtr& req,
                           drogon::FilterCallback&& fcb,
                           drogon::FilterChainCallback&& fccb) {
  auto& options_state = karing::options::runtime_options::instance();
  if (options_state.no_auth()) return fccb();

  // Determine method class early to allow localhost read-only fast-path
  auto method = req->getMethod();
  bool write_method = !(method==drogon::Get || method==drogon::Head || method==drogon::Options);
  if (method==drogon::Post) {
    const auto& p = req->path();
    if (p == "/search") write_method = false; // treat as read
  }

  // Determine client IP with proxy chain handling
  std::string peer_ip = req->getPeerAddr().toIp();
  std::string client_ip = peer_ip;
  if (options_state.trust_proxy()) {
    std::vector<std::string> chain;
    auto xff = req->getHeader("x-forwarded-for");
    if (!xff.empty()) {
      size_t pos=0; while (pos<=xff.size()) { size_t q=xff.find(',', pos); if (q==std::string::npos) q=xff.size(); std::string token=xff.substr(pos,q-pos); pos=q+1; while(!token.empty()&&token.front()==' ') token.erase(token.begin()); while(!token.empty()&&token.back()==' ') token.pop_back(); if(!token.empty()) chain.push_back(token);} }
    chain.push_back(peer_ip);
    const auto& trusted = options_state.trusted_proxies();
    if (!trusted.empty()) {
      for (int i=(int)chain.size()-1; i>=0; --i) { if (!ip_in_list(trusted, chain[(size_t)i])) { client_ip = chain[(size_t)i]; break; } }
      if (client_ip.empty()) client_ip = chain.front();
    } else { if (!chain.empty()) client_ip = chain.front(); }
    auto xrip = req->getHeader("x-real-ip"); if (!xrip.empty()) client_ip = xrip;
  }

  // Open DB
  sqlite3* db=nullptr; if (sqlite3_open_v2(options_state.db_path().c_str(), &db, SQLITE_OPEN_READWRITE, nullptr)!=SQLITE_OK) {
    // Return explicit JSON error to aid debugging
    auto resp = karing::http::error(HttpStatusCode::k500InternalServerError, "E_DB", "Database open failed");
    fcb(resp); if (db) sqlite3_close(db); return;
  }

  // Localhost fast-path (optional): only for read methods
  if (options_state.allow_localhost()) {
    bool is_loopback = (client_ip == "::1") || cidr_match("127.0.0.0/8", client_ip);
    if (is_loopback && !write_method) { if (db) sqlite3_close(db); return fccb(); }
  }

  // Load denies first (deny wins)
  auto denies = load_cidrs(db, "ip_deny");
  if (ip_in_list(denies, client_ip)) {
    sqlite3_close(db);
    Json::Value det; det["ip"] = client_ip; det["reason"] = "Matched deny list";
    auto resp = karing::http::error(HttpStatusCode::k403Forbidden, "E_IP_DENY", "Access denied by IP policy", det);
    fcb(resp); return;
  }

  // Allow list: if client IP is allowed, bypass API key authentication regardless of key presence/validity
  auto allows = load_cidrs(db, "ip_allow");
  if (ip_in_list(allows, client_ip)) { sqlite3_close(db); return fccb(); }

  // If no API keys exist, allow (IP only mode)
  long long keyCount = 0; sqlite3_stmt* cst=nullptr;
  if (sqlite3_prepare_v2(db, "SELECT COUNT(1) FROM api_keys;", -1, &cst, nullptr)==SQLITE_OK) {
    if (sqlite3_step(cst)==SQLITE_ROW) keyCount = sqlite3_column_int64(cst, 0);
  }
  if (cst) sqlite3_finalize(cst);
  if (keyCount==0) { sqlite3_close(db); return fccb(); }

  // Decide required role by method/path
  const auto& path = req->path();
  bool is_admin_path = (path.rfind("/admin/", 0) == 0);
  bool is_search = (path == "/search");
  bool need_write = write_method && !is_search; // POST/PUT/PATCH/DELETE except POST /search
  bool need_admin = is_admin_path;

  // API key required
  auto key = get_api_key(req);
  if (!key) {
    sqlite3_close(db);
    Json::Value det; det["hint"] = "Provide API key via X-API-Key header or ?api_key= query."; det["required"] = need_admin?"admin":(need_write?"write":"read");
    auto resp = karing::http::error(HttpStatusCode::k401Unauthorized, "E_NO_API_KEY", "API key required", det);
    fcb(resp); return;
  }

  // Validate + role
  sqlite3_stmt* st=nullptr; bool ok=false; int key_id=0; std::string role="write";
  if (sqlite3_prepare_v2(db, "SELECT id, role FROM api_keys WHERE key=? AND enabled=1;", -1, &st, nullptr)==SQLITE_OK) {
    sqlite3_bind_text(st, 1, key->c_str(), -1, SQLITE_TRANSIENT);
    if (sqlite3_step(st)==SQLITE_ROW) { ok=true; key_id = sqlite3_column_int(st, 0); if (sqlite3_column_type(st,1)!=SQLITE_NULL) { if (const unsigned char* t=sqlite3_column_text(st,1)) role=reinterpret_cast<const char*>(t); } }
  }
  if (st) sqlite3_finalize(st);
  if (!ok) {
    sqlite3_close(db);
    Json::Value det; det["hint"] = "Key not found or disabled";
    auto resp = karing::http::error(HttpStatusCode::k401Unauthorized, "E_INVALID_API_KEY", "Invalid API key", det);
    fcb(resp); return;
  }
  // Enforce role hierarchy: read < write < admin
  if (need_admin) {
    if (role != "admin") {
      sqlite3_close(db);
      Json::Value det; det["role"] = role; det["required"] = "admin";
      auto resp = karing::http::error(HttpStatusCode::k403Forbidden, "E_FORBIDDEN", "Insufficient role", det);
      fcb(resp); return;
    }
  } else if (need_write) {
    if (!(role=="write" || role=="admin")) {
      sqlite3_close(db);
      Json::Value det; det["role"] = role; det["required"] = "write or admin";
      auto resp = karing::http::error(HttpStatusCode::k403Forbidden, "E_FORBIDDEN", "Insufficient role", det);
      fcb(resp); return;
    }
  } else {
    // read requires any valid key (read/write/admin)
    if (!(role=="read" || role=="write" || role=="admin")) {
      sqlite3_close(db);
      Json::Value det; det["role"] = role; det["required"] = "read or higher";
      auto resp = karing::http::error(HttpStatusCode::k403Forbidden, "E_FORBIDDEN", "Insufficient role", det);
      fcb(resp); return;
    }
  }

  // Update usage
  sqlite3_exec(db, "BEGIN;", nullptr, nullptr, nullptr);
  sqlite3_stmt* us=nullptr;
  if (sqlite3_prepare_v2(db, "UPDATE api_keys SET last_used_at=strftime('%s','now'), last_ip=? WHERE id=?;", -1, &us, nullptr)==SQLITE_OK) {
    sqlite3_bind_text(us, 1, client_ip.c_str(), -1, SQLITE_TRANSIENT);
    sqlite3_bind_int(us, 2, key_id);
    sqlite3_step(us);
  }
  if (us) sqlite3_finalize(us);
  sqlite3_exec(db, "COMMIT;", nullptr, nullptr, nullptr);
  sqlite3_close(db);
  return fccb();
}

}

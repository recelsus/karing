#include "admin_filter.h"
#include <drogon/drogon.h>
#include <sqlite3.h>
#include <string>
#include <optional>
#include <vector>
#include <algorithm>
#include <cctype>
#include <memory>
#include "utils/options.h"
#include "utils/json_response.h"

using drogon::HttpRequestPtr;
using drogon::HttpStatusCode;

namespace karing::filters {

static bool parse_ipv4(const std::string& ip, uint32_t& out) {
  unsigned a, b, c, d; char ch;
  if (sscanf(ip.c_str(), "%u.%u.%u.%u%c", &a, &b, &c, &d, &ch) == 4 &&
      a < 256 && b < 256 && c < 256 && d < 256) {
    out = (a << 24) | (b << 16) | (c << 8) | d;
    return true;
  }
  return false;
}

static bool cidr_match(const std::string& cidr, const std::string& ip) {
  auto pos = cidr.find('/');
  if (pos == std::string::npos) return cidr == ip;
  int bits = 0;
  try {
    bits = std::stoi(cidr.substr(pos + 1));
  } catch (...) {
    return false;
  }
  std::string net = cidr.substr(0, pos);
  uint32_t nip, tip;
  if (!parse_ipv4(net, nip) || !parse_ipv4(ip, tip)) return false;
  if (bits <= 0) return true;
  if (bits >= 32) return tip == nip;
  uint32_t mask = bits == 0 ? 0U : htonl(~((1U << (32 - bits)) - 1));
  return (tip & mask) == (nip & mask);
}

static bool ip_in_list(const std::vector<std::string>& cidrs, const std::string& ip) {
  for (const auto& c : cidrs) {
    if (cidr_match(c, ip)) return true;
  }
  return false;
}

void admin_filter::doFilter(const HttpRequestPtr& req,
                            drogon::FilterCallback&& fcb,
                            drogon::FilterChainCallback&& fccb) {
  auto& options_state = karing::options::runtime_options::instance();
  std::string peer_ip = req->getPeerAddr().toIp();
  std::string client_ip = peer_ip;
  if (options_state.trust_proxy()) {
    std::vector<std::string> chain;
    auto xff = req->getHeader("x-forwarded-for");
    if (!xff.empty()) {
      size_t pos = 0;
      while (pos <= xff.size()) {
        size_t q = xff.find(',', pos);
        if (q == std::string::npos) q = xff.size();
        std::string token = xff.substr(pos, q - pos);
        pos = q + 1;
        while (!token.empty() && token.front() == ' ') token.erase(token.begin());
        while (!token.empty() && token.back() == ' ') token.pop_back();
        if (!token.empty()) chain.push_back(token);
      }
    }
    chain.push_back(peer_ip);
    const auto& trusted = options_state.trusted_proxies();
    if (!trusted.empty()) {
      for (int i = static_cast<int>(chain.size()) - 1; i >= 0; --i) {
        if (!ip_in_list(trusted, chain[static_cast<size_t>(i)])) {
          client_ip = chain[static_cast<size_t>(i)];
          break;
        }
      }
      if (client_ip.empty() && !chain.empty()) client_ip = chain.front();
    } else if (!chain.empty()) {
      client_ip = chain.front();
    }
    auto xrip = req->getHeader("x-real-ip");
    if (!xrip.empty()) client_ip = xrip;
  }

  sqlite3* raw_db = nullptr;
  if (sqlite3_open_v2(options_state.db_path().c_str(), &raw_db, SQLITE_OPEN_READONLY, nullptr) != SQLITE_OK) {
    auto resp = karing::http::error(HttpStatusCode::k500InternalServerError, "E_DB", "Database open failed");
    fcb(resp);
    if (raw_db) sqlite3_close(raw_db);
    return;
  }
  std::unique_ptr<sqlite3, decltype(&sqlite3_close)> db(raw_db, sqlite3_close);
  bool allowed = false;
  sqlite3_stmt* st = nullptr;
  const char* sql = "SELECT pattern FROM ip_rules WHERE permission='allow' AND enabled=1;";
  if (sqlite3_prepare_v2(db.get(), sql, -1, &st, nullptr) == SQLITE_OK) {
    while (sqlite3_step(st) == SQLITE_ROW) {
      if (const unsigned char* t = sqlite3_column_text(st, 0)) {
        if (cidr_match(reinterpret_cast<const char*>(t), client_ip)) {
          allowed = true;
          break;
        }
      }
    }
  }
  if (st) sqlite3_finalize(st);

  if (!allowed) {
    Json::Value det;
    det["ip"] = client_ip;
    auto resp = karing::http::error(HttpStatusCode::k403Forbidden,
                                    "E_ADMIN_IP",
                                    "Admin endpoints require explicit allow-listed IP",
                                    det);
    fcb(resp);
    return;
  }

  return fccb();
}

}

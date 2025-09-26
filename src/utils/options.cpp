#include "options.h"
#include <mutex>

namespace karing::options {

namespace {
std::string g_db_path;
int g_build_limit = 100;
int g_runtime_limit = 100;
int g_max_file_bytes = 20971520;
int g_max_text_bytes = 10485760;
bool g_no_auth = false;
bool g_trust_proxy = false;
bool g_allow_localhost = false;
bool g_tls_enabled = false;
bool g_tls_require = false;
int g_tls_https_port = 0;
int g_tls_http_port = 0;
std::vector<std::string> g_trusted_proxies;
std::string g_tls_cert;
std::string g_tls_key;
std::string g_base_path = "/";
std::mutex g_mtx;
}

void set_runtime(const std::string& db_path, int build_limit, int runtime_limit,
                 int max_file_bytes, int max_text_bytes,
                 bool no_auth, bool trust_proxy,
                 bool allow_localhost) {
  std::lock_guard<std::mutex> lk(g_mtx);
  g_db_path = db_path;
  g_build_limit = build_limit;
  g_runtime_limit = runtime_limit;
  g_max_file_bytes = max_file_bytes;
  g_max_text_bytes = max_text_bytes;
  g_no_auth = no_auth;
  g_trust_proxy = trust_proxy;
  g_allow_localhost = allow_localhost;
}

const std::string& db_path() { return g_db_path; }
int build_limit() { return g_build_limit; }
int runtime_limit() { return g_runtime_limit; }
int max_file_bytes() { return g_max_file_bytes; }
int max_text_bytes() { return g_max_text_bytes; }
bool no_auth() { return g_no_auth; }
bool trust_proxy() { return g_trust_proxy; }
bool allow_localhost() { return g_allow_localhost; }

void set_tls(bool enabled, bool require, int https_port, int http_port) {
  std::lock_guard<std::mutex> lk(g_mtx);
  g_tls_enabled = enabled; g_tls_require = require;
  g_tls_https_port = https_port; g_tls_http_port = http_port;
}
bool tls_enabled() { return g_tls_enabled; }
bool tls_require() { return g_tls_require; }
int tls_https_port() { return g_tls_https_port; }
int tls_http_port() { return g_tls_http_port; }

void set_trusted_proxies(const std::vector<std::string>& cidrs) {
  std::lock_guard<std::mutex> lk(g_mtx);
  g_trusted_proxies = cidrs;
}
const std::vector<std::string>& trusted_proxies() { return g_trusted_proxies; }

void set_tls_cert_paths(const std::string& cert, const std::string& key) {
  std::lock_guard<std::mutex> lk(g_mtx);
  g_tls_cert = cert; g_tls_key = key;
}
const std::string& tls_cert_path() { return g_tls_cert; }
const std::string& tls_key_path() { return g_tls_key; }

void set_base_path(const std::string& path) {
  std::lock_guard<std::mutex> lk(g_mtx);
  g_base_path = path.empty()? "/" : path;
  if (g_base_path.back()=='/' && g_base_path.size()>1) g_base_path.pop_back();
}
const std::string& base_path() { return g_base_path; }

}

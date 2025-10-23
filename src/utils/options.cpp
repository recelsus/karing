#include "options.h"

namespace karing::options {

runtime_options& runtime_options::instance() {
  static runtime_options runtime_state;
  return runtime_state;
}

void runtime_options::set_runtime(const std::string& db_path_value, int build_limit_value, int runtime_limit_value,
                                  int max_file_bytes_value, int max_text_bytes_value,
                                  bool no_auth_value, bool trust_proxy_value,
                                  bool allow_localhost_value) {
  std::lock_guard<std::mutex> guard(state_mutex);
  db_path_storage = db_path_value;
  build_limit_storage = build_limit_value;
  runtime_limit_storage = runtime_limit_value;
  max_file_bytes_storage = max_file_bytes_value;
  max_text_bytes_storage = max_text_bytes_value;
  no_auth_storage = no_auth_value;
  trust_proxy_storage = trust_proxy_value;
  allow_localhost_storage = allow_localhost_value;
}

const std::string& runtime_options::db_path() const { return db_path_storage; }
int runtime_options::build_limit() const { return build_limit_storage; }
int runtime_options::runtime_limit() const { return runtime_limit_storage; }
int runtime_options::max_file_bytes() const { return max_file_bytes_storage; }
int runtime_options::max_text_bytes() const { return max_text_bytes_storage; }
bool runtime_options::no_auth() const { return no_auth_storage; }
bool runtime_options::trust_proxy() const { return trust_proxy_storage; }
bool runtime_options::allow_localhost() const { return allow_localhost_storage; }

void runtime_options::set_tls(bool enabled_value, bool require_value, int https_port_value, int http_port_value) {
  std::lock_guard<std::mutex> guard(state_mutex);
  tls_enabled_storage = enabled_value;
  tls_require_storage = require_value;
  tls_https_port_storage = https_port_value;
  tls_http_port_storage = http_port_value;
}

bool runtime_options::tls_enabled() const { return tls_enabled_storage; }
bool runtime_options::tls_require() const { return tls_require_storage; }
int runtime_options::tls_https_port() const { return tls_https_port_storage; }
int runtime_options::tls_http_port() const { return tls_http_port_storage; }

void runtime_options::set_tls_cert_paths(const std::string& cert_path, const std::string& key_path) {
  std::lock_guard<std::mutex> guard(state_mutex);
  tls_cert_path_storage = cert_path;
  tls_key_path_storage = key_path;
}

const std::string& runtime_options::tls_cert_path() const { return tls_cert_path_storage; }
const std::string& runtime_options::tls_key_path() const { return tls_key_path_storage; }

void runtime_options::set_trusted_proxies(const std::vector<std::string>& cidr_values) {
  std::lock_guard<std::mutex> guard(state_mutex);
  trusted_proxies_storage = cidr_values;
}

const std::vector<std::string>& runtime_options::trusted_proxies() const { return trusted_proxies_storage; }

void runtime_options::set_base_path(const std::string& path_value) {
  std::lock_guard<std::mutex> guard(state_mutex);
  base_path_storage = path_value.empty() ? "/" : path_value;
  if (base_path_storage.size() > 1 && base_path_storage.back() == '/') {
    base_path_storage.pop_back();
  }
}

const std::string& runtime_options::base_path() const { return base_path_storage; }

}

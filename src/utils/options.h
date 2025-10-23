#pragma once

#include <mutex>
#include <string>
#include <vector>

namespace karing::options {

class runtime_options {
public:
  static runtime_options& instance();

  void set_runtime(const std::string& db_path_value, int build_limit_value, int runtime_limit_value,
                   int max_file_bytes_value, int max_text_bytes_value,
                   bool no_auth_value = false, bool trust_proxy_value = false,
                   bool allow_localhost_value = false);

  const std::string& db_path() const;
  int build_limit() const;
  int runtime_limit() const;
  int max_file_bytes() const;
  int max_text_bytes() const;
  bool no_auth() const;
  bool trust_proxy() const;
  bool allow_localhost() const;

  void set_tls(bool enabled_value, bool require_value, int https_port_value, int http_port_value);
  bool tls_enabled() const;
  bool tls_require() const;
  int tls_https_port() const;
  int tls_http_port() const;
  void set_tls_cert_paths(const std::string& cert_path, const std::string& key_path);
  const std::string& tls_cert_path() const;
  const std::string& tls_key_path() const;

  void set_trusted_proxies(const std::vector<std::string>& cidr_values);
  const std::vector<std::string>& trusted_proxies() const;

  void set_base_path(const std::string& path_value);
  const std::string& base_path() const;

private:
  runtime_options() = default;
  runtime_options(const runtime_options&) = delete;
  runtime_options& operator=(const runtime_options&) = delete;

  std::string db_path_storage;
  int build_limit_storage = 100;
  int runtime_limit_storage = 100;
  int max_file_bytes_storage = 20971520;
  int max_text_bytes_storage = 10485760;
  bool no_auth_storage = false;
  bool trust_proxy_storage = false;
  bool allow_localhost_storage = false;
  bool tls_enabled_storage = false;
  bool tls_require_storage = false;
  int tls_https_port_storage = 0;
  int tls_http_port_storage = 0;
  std::vector<std::string> trusted_proxies_storage;
  std::string tls_cert_path_storage;
  std::string tls_key_path_storage;
  std::string base_path_storage = "/";
  mutable std::mutex state_mutex;
};

}

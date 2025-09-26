#pragma once
#include <string>
#include <vector>

namespace karing::options {

void set_runtime(const std::string& db_path, int build_limit, int runtime_limit,
                 int max_file_bytes, int max_text_bytes,
                 bool no_auth = false, bool trust_proxy = false,
                 bool allow_localhost = false);

const std::string& db_path();
int build_limit();
int runtime_limit();
int max_file_bytes();
int max_text_bytes();
bool no_auth();
bool trust_proxy();
bool allow_localhost();

// TLS flags
void set_tls(bool enabled, bool require, int https_port, int http_port);
bool tls_enabled();
bool tls_require();
int tls_https_port();
int tls_http_port();
void set_tls_cert_paths(const std::string& cert, const std::string& key);
const std::string& tls_cert_path();
const std::string& tls_key_path();

// Trusted proxy CIDRs for resolving client IP from X-Forwarded-For chain
void set_trusted_proxies(const std::vector<std::string>& cidrs);
const std::vector<std::string>& trusted_proxies();

// Base path (for reverse proxy path prefixes, e.g. "/api")
void set_base_path(const std::string& path);
const std::string& base_path();

}

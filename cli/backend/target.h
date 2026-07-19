#pragma once

#include <string>

namespace karing::cli::backend {

enum class target_kind {
  http,
  sqlite,
};

struct target {
  target_kind kind{target_kind::sqlite};
  std::string value;
};

target parse_target(const std::string& raw);
bool is_http_target(const std::string& raw);
int print_unsupported_sqlite_command(const std::string& command);

}  // namespace karing::cli::backend

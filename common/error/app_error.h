#pragma once

#include <optional>
#include <string>

namespace karing::domain {

enum class error_category {
  none,
  validation,
  query,
  not_found,
  conflict,
  size,
  mime,
  backend,
  database,
  unavailable,
  internal,
};

enum class error_code {
  none,
  validation,
  query,
  not_found,
  conflict,
  size,
  mime,
  backend_unsupported,
  sqlite_open_failed,
  sqlite_configure_failed,
  sqlite_busy,
  sqlite_constraint,
  sqlite_io,
  sqlite_unknown,
  fts_unavailable,
  internal,
};

struct app_error {
  error_category category{error_category::none};
  error_code code{error_code::none};
  std::string message;
  std::optional<std::string> detail;
};

app_error make_error(error_category category,
                     error_code code,
                     std::string message,
                     std::optional<std::string> detail = std::nullopt);

const char* to_code_string(error_code code);
const char* to_category_string(error_category category);

}  // namespace karing::domain

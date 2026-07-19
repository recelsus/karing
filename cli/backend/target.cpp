#include "backend/target.h"

#include <algorithm>
#include <cctype>

#include "utils/io.h"
#include "utils/url.h"

namespace karing::cli::backend {

namespace {

std::string to_lower_ascii(std::string value) {
  std::transform(value.begin(), value.end(), value.begin(), [](unsigned char ch) {
    return static_cast<char>(std::tolower(ch));
  });
  return value;
}

bool starts_with(const std::string& value, const char* prefix) {
  const std::string needle(prefix);
  return value.rfind(needle, 0) == 0;
}

}  // namespace

bool is_http_target(const std::string& raw) {
  const auto value = to_lower_ascii(raw);
  return starts_with(value, "http://") || starts_with(value, "https://");
}

target parse_target(const std::string& raw) {
  target out;
  if (is_http_target(raw)) {
    out.kind = target_kind::http;
    out.value = utils::normalize_base_url(raw);
    return out;
  }

  out.kind = target_kind::sqlite;
  out.value = raw;
  return out;
}

int print_unsupported_sqlite_command(const std::string& command) {
  return utils::print_error("sqlite backend command is not implemented yet: " + command);
}

}  // namespace karing::cli::backend

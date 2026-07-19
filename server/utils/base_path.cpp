#include "utils/base_path.h"

namespace karing::base_path {

namespace {

std::string strip_query_and_fragment(const std::string& value) {
  const auto pos = value.find_first_of("?#");
  return pos == std::string::npos ? value : value.substr(0, pos);
}

std::string path_from_url(const std::string& raw) {
  const auto scheme_pos = raw.find("://");
  if (scheme_pos == std::string::npos) return raw;

  const auto authority_start = scheme_pos + 3;
  const auto path_start = raw.find('/', authority_start);
  if (path_start == std::string::npos) return "/";
  return raw.substr(path_start);
}

}  // namespace

std::string normalize(const std::string& raw) {
  std::string value = strip_query_and_fragment(path_from_url(raw));
  if (value.empty()) return "/";
  if (value.front() != '/') value.insert(value.begin(), '/');
  while (value.size() > 1 && value.back() == '/') value.pop_back();
  return value.empty() ? "/" : value;
}

}  // namespace karing::base_path

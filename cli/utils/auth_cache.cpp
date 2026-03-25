#include "utils/auth_cache.h"

#include <filesystem>
#include <fstream>
#include <functional>

namespace karing::cli::utils {

namespace {

std::filesystem::path cache_path(const std::string& base_url) {
  const auto key = std::to_string(std::hash<std::string>{}(base_url));
  return std::filesystem::temp_directory_path() / ("karing-cli-auth-" + key + ".cache");
}

}  // namespace

std::optional<auth_scheme> load_auth_scheme(const std::string& base_url) {
  std::ifstream in(cache_path(base_url));
  std::string value;
  if (!(in >> value)) return std::nullopt;
  if (value == "bearer") return auth_scheme::bearer;
  if (value == "x-api-key") return auth_scheme::x_api_key;
  return std::nullopt;
}

void save_auth_scheme(const std::string& base_url, auth_scheme scheme) {
  std::ofstream out(cache_path(base_url), std::ios::trunc);
  out << (scheme == auth_scheme::bearer ? "bearer" : "x-api-key");
}

}

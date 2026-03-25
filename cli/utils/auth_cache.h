#pragma once

#include <optional>
#include <string>

namespace karing::cli::utils {

enum class auth_scheme {
  bearer,
  x_api_key,
};

std::optional<auth_scheme> load_auth_scheme(const std::string& base_url);
void save_auth_scheme(const std::string& base_url, auth_scheme scheme);

}

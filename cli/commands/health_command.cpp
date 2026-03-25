#include "commands/commands.h"

#include "http/http_client.h"
#include "utils/io.h"

namespace karing::cli::commands {

int run_health(const std::string& base_url, const std::optional<std::string>& api_key, bool json_output) {
  const auto response = http::get(base_url, "/health", {}, api_key);
  return json_output ? utils::print_response_json(response) : utils::print_response(response, false);
}

}

#include "commands/commands.h"

#include <map>
#include <string>

#include "http/http_client.h"
#include "utils/io.h"

namespace karing::cli::commands {

int run_get(const std::string& base_url,
            const std::optional<std::string>& api_key,
            std::optional<int> id,
            bool json_output) {
  std::map<std::string, std::string> query;
  if (id.has_value()) query.emplace("id", std::to_string(*id));
  if (json_output) query.emplace("json", "true");
  const auto response = http::get(base_url, "/", query, api_key);
  if (json_output) return utils::print_response_json(response);
  return utils::print_response(response, true);
}

}

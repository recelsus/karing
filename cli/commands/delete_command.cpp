#include "commands/commands.h"

#include <map>
#include <string>

#include "http/http_client.h"
#include "utils/io.h"

namespace karing::cli::commands {

int run_delete(const std::string& base_url,
               const std::optional<std::string>& api_key,
               bool json_output,
               const std::vector<std::string>& args) {
  std::map<std::string, std::string> query;
  if (!args.empty()) query.emplace("id", args.front());
  const auto response = http::del(base_url, "/", query, api_key);
  return json_output ? utils::print_response_json(response) : utils::print_response(response, false);
}

}

#include "commands/commands.h"

#include <json/json.h>

#include <iostream>
#include <map>
#include <sstream>
#include <string>

#include "http/http_client.h"
#include "utils/arg_utils.h"
#include "utils/io.h"

namespace karing::cli::commands {

int run_move(const std::string& base_url,
             const std::optional<std::string>& api_key,
             bool json_output,
             const std::vector<std::string>& args) {
  if (args.size() != 2) return utils::print_error("move requires an id and a before-id");
  if (!utils::is_valid_id(args[0]) || !utils::is_valid_id(args[1])) {
    return utils::print_error("move ids must be in range 1..1000");
  }
  if (args[0] == args[1]) return utils::print_error("move ids must be different");

  std::map<std::string, std::string> query;
  query["id"] = args[0];
  query["before"] = args[1];
  const auto response = http::post(base_url, "/move", query, api_key);
  if (json_output) return utils::print_response_json(response);
  if (!(response.status >= 200 && response.status < 300)) return utils::print_response(response, false);

  Json::CharReaderBuilder builder;
  Json::Value root;
  std::string errors;
  std::istringstream input(response.body);
  if (!Json::parseFromStream(builder, input, &root, &errors)) return utils::print_response(response, false);

  const auto count = root["meta"]["count"].isInt() ? root["meta"]["count"].asInt() : 0;
  const auto next_id = root["meta"]["next_id"].isInt() ? root["meta"]["next_id"].asInt() : 0;
  std::cout << "move complete\n";
  std::cout << "count: " << count << '\n';
  std::cout << "next_id: " << next_id << '\n';
  return 0;
}

}  // namespace karing::cli::commands

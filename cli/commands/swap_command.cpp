#include "commands/commands.h"

#include <map>
#include <string>

#include "http/http_client.h"
#include "utils/io.h"

namespace karing::cli::commands {

namespace {

bool is_valid_id(const std::string& value) {
  if (value.empty()) return false;
  for (char ch : value) {
    if (ch < '0' || ch > '9') return false;
  }
  try {
    const int id = std::stoi(value);
    return id >= 1 && id <= 1000;
  } catch (...) {
    return false;
  }
}

}  // namespace

int run_swap(const std::string& base_url,
             const std::optional<std::string>& api_key,
             bool json_output,
             const std::vector<std::string>& args) {
  if (args.size() != 2) return utils::print_error("swap requires two ids");
  if (!is_valid_id(args[0]) || !is_valid_id(args[1])) {
    return utils::print_error("swap ids must be in range 1..1000");
  }
  if (args[0] == args[1]) return utils::print_error("swap ids must be different");

  std::map<std::string, std::string> query;
  query["id1"] = args[0];
  query["id2"] = args[1];
  const auto response = http::post(base_url, "/swap", query, api_key);
  return json_output ? utils::print_response_json(response) : utils::print_response(response, false);
}

}

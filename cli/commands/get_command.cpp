#include "commands/commands.h"

#include <iostream>
#include <map>
#include <sstream>
#include <string>

#include <json/json.h>

#include "http/http_client.h"
#include "utils/io.h"

namespace karing::cli::commands {

namespace {

bool load_record_metadata(const http::response& response, Json::Value& out_record) {
  if (response.status < 200 || response.status >= 300) return false;
  Json::CharReaderBuilder builder;
  Json::Value root;
  std::string errors;
  std::istringstream input(response.body);
  if (!Json::parseFromStream(builder, input, &root, &errors)) return false;
  if (!root.isObject() || !root["data"].isArray() || root["data"].empty()) return false;
  out_record = root["data"][0];
  return true;
}

std::string download_url(const std::string& base_url, int id) {
  return base_url + "/?id=" + std::to_string(id) + "&as=download";
}

int print_binary_download_hint(const std::string& base_url, const Json::Value& record) {
  const int id = record.get("id", 0).asInt();
  const std::string filename = record.get("filename", "").asString();
  const std::string mime = record.get("mime", "").asString();
  const auto url = download_url(base_url, id);

  std::cout
      << "Binary file detected.\n"
      << "id: " << id << '\n';
  if (!filename.empty()) std::cout << "filename: " << filename << '\n';
  if (!mime.empty()) std::cout << "mime: " << mime << '\n';
  std::cout
      << "download:\n"
      << "  curl -L -OJ '" << url << "'\n"
      << "  wget --content-disposition '" << url << "'\n";
  return 0;
}

}  // namespace

int run_get(const std::string& base_url,
            const std::optional<std::string>& api_key,
            std::optional<int> id,
            bool json_output) {
  std::map<std::string, std::string> query;
  if (id.has_value()) query.emplace("id", std::to_string(*id));
  if (json_output) {
    query.emplace("json", "true");
    const auto response = http::get(base_url, "/", query, api_key);
    return utils::print_response_json(response);
  }

  auto metadata_query = query;
  metadata_query.emplace("json", "true");
  const auto metadata_response = http::get(base_url, "/", metadata_query, api_key);
  Json::Value record;
  if (load_record_metadata(metadata_response, record) && record.get("is_file", false).asBool()) {
    return print_binary_download_hint(base_url, record);
  }

  const auto response = http::get(base_url, "/", query, api_key);
  return utils::print_response(response, true);
}

}

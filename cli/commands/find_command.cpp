#include "commands/commands.h"

#include <iomanip>
#include <iostream>
#include <map>
#include <optional>
#include <sstream>
#include <string>
#include <ctime>
#include <vector>

#include <json/json.h>

#include "http/http_client.h"
#include "utils/io.h"

namespace karing::cli::commands {

namespace {

std::string join_words(const std::vector<std::string>& items) {
  std::string out;
  for (size_t i = 0; i < items.size(); ++i) {
    if (i > 0) out += ' ';
    out += items[i];
  }
  return out;
}

std::string truncate_content(const std::string& value, bool full) {
  if (full || value.size() <= 48) return value;
  return value.substr(0, 45) + "...";
}

std::string format_timestamp(const Json::Value& value) {
  if (!value.isInt64() && !value.isUInt64() && !value.isInt() && !value.isUInt()) return "-";
  const std::time_t ts = static_cast<std::time_t>(value.asInt64());
  std::tm tm{};
#if defined(_WIN32)
  if (gmtime_s(&tm, &ts) != 0) return "-";
#else
  if (gmtime_r(&ts, &tm) == nullptr) return "-";
#endif
  std::ostringstream out;
  out << std::put_time(&tm, "%Y-%m-%d %H:%M:%S");
  return out.str();
}

int print_find_table(const http::response& response, bool full) {
  Json::CharReaderBuilder builder;
  Json::Value root;
  std::string errors;
  std::istringstream input(response.body);
  if (!Json::parseFromStream(builder, input, &root, &errors)) {
    return utils::print_error("failed to parse /search response as JSON");
  }
  if (!root.isObject() || !root["data"].isArray()) {
    return utils::print_error("unexpected /search response shape");
  }

  constexpr int kIdWidth = 6;
  constexpr int kTypeWidth = 24;
  constexpr int kContentWidth = 48;
  constexpr int kCreatedWidth = 21;
  constexpr int kUpdatedWidth = 21;

  std::cout << std::left
            << std::setw(kIdWidth) << "id"
            << std::setw(kTypeWidth) << "type"
            << std::setw(kContentWidth) << "content"
            << std::setw(kCreatedWidth) << "created_at"
            << std::setw(kUpdatedWidth) << "updated_at"
            << '\n';
  std::cout << std::string(kIdWidth + kTypeWidth + kContentWidth + kCreatedWidth + kUpdatedWidth, '-')
            << '\n';

  for (const auto& item : root["data"]) {
    const bool is_file = item.get("is_file", false).asBool();
    const bool has_filename = item.isMember("filename") && !item["filename"].asString().empty();
    std::string type;
    std::string content;
    if (is_file) {
      type = item.get("mime", "").asString();
      content = item.get("filename", "").asString();
    } else if (has_filename) {
      type = "text(file)";
      content = item.get("filename", "").asString();
    } else {
      type = "text";
      content = item.get("content", "").asString();
    }
    const std::string created_at = item.isMember("created_at") ? format_timestamp(item["created_at"]) : "-";
    const std::string updated_at = item.isMember("updated_at") ? format_timestamp(item["updated_at"]) : "-";

    std::cout << std::left
              << std::setw(kIdWidth) << item.get("id", 0).asInt()
              << std::setw(kTypeWidth) << type
              << std::setw(kContentWidth) << truncate_content(content, full)
              << std::setw(kCreatedWidth) << created_at
              << std::setw(kUpdatedWidth) << updated_at
              << '\n';
  }

  return 0;
}

}  // namespace

int run_find(const std::string& base_url,
             const std::optional<std::string>& api_key,
             bool json_output,
             const std::vector<std::string>& args) {
  std::map<std::string, std::string> query;
  std::vector<std::string> terms;
  bool asc = false;
  bool desc = false;
  bool full = false;

  for (size_t i = 0; i < args.size(); ++i) {
    const auto& arg = args[i];
    if ((arg == "--limit" || arg == "-l") && i + 1 < args.size()) {
      query["limit"] = args[++i];
      continue;
    }
    if ((arg == "--type" || arg == "-t") && i + 1 < args.size()) {
      query["type"] = args[++i];
      continue;
    }
    if ((arg == "--sort" || arg == "-s") && i + 1 < args.size()) {
      std::string value = args[++i];
      if (value == "store") value = "stored_at";
      else if (value == "update") value = "updated_at";
      query["sort"] = value;
      continue;
    }
    if (arg == "--asc") {
      asc = true;
      continue;
    }
    if (arg == "--desc") {
      desc = true;
      continue;
    }
    if (arg == "--full") {
      full = true;
      continue;
    }
    terms.push_back(arg);
  }

  if (asc && desc) return utils::print_error("--asc and --desc cannot be used together");
  query["order"] = asc ? "asc" : "desc";
  const std::string q = join_words(terms);
  if (!q.empty()) query["q"] = q;

  const auto response = http::get(base_url, "/search", query, api_key);
  if (json_output) return utils::print_response_json(response);
  if (response.status < 200 || response.status >= 300) return utils::print_response(response, false);
  return print_find_table(response, full);
}

}

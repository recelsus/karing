#include "commands/commands.h"

#include <filesystem>
#include <optional>
#include <string>
#include <vector>

#include <json/json.h>

#include "http/http_client.h"
#include "utils/arg_utils.h"
#include "utils/io.h"
#include "utils/mime.h"

namespace karing::cli::commands {

int run_mod(const std::string& base_url,
            const std::optional<std::string>& api_key,
            bool json_output,
            const std::vector<std::string>& args) {
  if (args.empty()) return utils::print_error("mod requires an id");
  const auto& id = args.front();
  if (!utils::is_valid_id(id)) return utils::print_error("mod id must be in range 1..1000");

  std::optional<std::string> file_path;
  std::optional<std::string> mime;
  std::optional<std::string> name;
  std::vector<std::string> text_parts;

  for (size_t i = 1; i < args.size(); ++i) {
    const auto& arg = args[i];
    if ((arg == "-f" || arg == "--file") && i + 1 < args.size()) {
      file_path = args[++i];
      continue;
    }
    if (arg == "--mime" && i + 1 < args.size()) {
      mime = args[++i];
      continue;
    }
    if (arg == "--name" && i + 1 < args.size()) {
      name = args[++i];
      continue;
    }
    text_parts.push_back(arg);
  }

  std::map<std::string, std::string> query;
  query["id"] = id;

  if (file_path.has_value()) {
    if (!std::filesystem::exists(*file_path)) return utils::print_error("file does not exist: " + *file_path);
    if (!mime.has_value()) mime = utils::guess_mime_type(*file_path);
    if (!name.has_value()) name = std::filesystem::path(*file_path).filename().string();
    const auto response = http::multipart_form(base_url, "/", "PUT", query, "file", *file_path, name, mime, api_key);
    return json_output ? utils::print_response_json(response) : utils::print_response(response, false);
  }

  std::string content = utils::join_words(text_parts);
  if (content.empty()) {
    if (const auto stdin_text = utils::read_stdin_text(); stdin_text.has_value()) content = *stdin_text;
  }
  if (content.empty()) return utils::print_error("text content or -f/--file is required");

  Json::Value body(Json::objectValue);
  body["content"] = content;
  Json::StreamWriterBuilder writer;
  writer["indentation"] = "";
  const auto response = http::put_json(base_url, "/", query, Json::writeString(writer, body), api_key);
  return json_output ? utils::print_response_json(response) : utils::print_response(response, false);
}

}

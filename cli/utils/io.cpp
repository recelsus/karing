#include "utils/io.h"

#include <json/json.h>
#include <unistd.h>

#include <iostream>
#include <sstream>

namespace karing::cli::utils {

bool stdin_has_data() {
  return !isatty(STDIN_FILENO);
}

std::optional<std::string> read_stdin_text() {
  if (!stdin_has_data()) return std::nullopt;
  std::ostringstream buffer;
  buffer << std::cin.rdbuf();
  return buffer.str();
}

namespace {

bool try_pretty_print_json(const std::string& body) {
  Json::CharReaderBuilder builder;
  Json::Value root;
  std::string errors;
  std::istringstream input(body);
  if (!Json::parseFromStream(builder, input, &root, &errors)) return false;
  Json::StreamWriterBuilder writer;
  writer["indentation"] = "  ";
  std::cout << Json::writeString(writer, root);
  if (body.empty() || body.back() != '\n') std::cout << '\n';
  return true;
}

}  // namespace

int print_response(const http::response& response, bool raw_stdout) {
  const bool ok = response.status >= 200 && response.status < 300;
  std::ostream& stream = ok ? std::cout : std::cerr;
  if (raw_stdout) {
    stream << response.body;
    return ok ? 0 : 1;
  }
  if (!response.body.empty() &&
      (response.content_type.find("application/json") != std::string::npos || response.body.front() == '{' || response.body.front() == '[')) {
    if (try_pretty_print_json(response.body)) return ok ? 0 : 1;
  }
  if (!response.body.empty()) stream << response.body << '\n';
  return ok ? 0 : 1;
}

int print_response_json(const http::response& response) {
  if (!response.body.empty()) std::cout << response.body;
  if (!response.body.empty() && response.body.back() != '\n') std::cout << '\n';
  return (response.status >= 200 && response.status < 300) ? 0 : 1;
}

int print_error(const std::string& message) {
  std::cerr << "ERROR: " << message << '\n';
  return 1;
}

}

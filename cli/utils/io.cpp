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

const char* status_text(long status) {
  switch (status) {
    case 200: return "OK";
    case 201: return "Created";
    case 204: return "No Content";
    case 400: return "Bad Request";
    case 401: return "Unauthorized";
    case 403: return "Forbidden";
    case 404: return "Not Found";
    case 409: return "Conflict";
    case 413: return "Payload Too Large";
    case 415: return "Unsupported Media Type";
    case 500: return "Internal Server Error";
    case 503: return "Service Unavailable";
    default: return "";
  }
}

void print_status_line(std::ostream& stream, const http::response& response) {
  stream << "HTTP " << response.status;
  const char* text = status_text(response.status);
  if (text[0] != '\0') stream << ' ' << text;
  stream << '\n';
}

bool try_pretty_print_json(std::ostream& stream, const std::string& body) {
  Json::CharReaderBuilder builder;
  Json::Value root;
  std::string errors;
  std::istringstream input(body);
  if (!Json::parseFromStream(builder, input, &root, &errors)) return false;
  Json::StreamWriterBuilder writer;
  writer["indentation"] = "  ";
  stream << Json::writeString(writer, root);
  if (body.empty() || body.back() != '\n') stream << '\n';
  return true;
}

}  // namespace

int print_response(const http::response& response, bool raw_stdout) {
  const bool ok = response.status >= 200 && response.status < 300;
  std::ostream& stream = ok ? std::cout : std::cerr;
  if (raw_stdout) {
    if (!ok) {
      if (!response.body.empty() &&
          (response.content_type.find("application/json") != std::string::npos || response.body.front() == '{' || response.body.front() == '[')) {
        if (try_pretty_print_json(stream, response.body)) return 1;
      }
      if (!response.body.empty()) {
        stream << response.body;
        if (response.body.back() != '\n') stream << '\n';
      } else {
        print_status_line(stream, response);
      }
      return 1;
    }
    stream << response.body;
    return ok ? 0 : 1;
  }
  if (!response.body.empty() &&
      (response.content_type.find("application/json") != std::string::npos || response.body.front() == '{' || response.body.front() == '[')) {
    if (try_pretty_print_json(stream, response.body)) return ok ? 0 : 1;
  }
  if (!response.body.empty()) stream << response.body << '\n';
  else if (!ok) print_status_line(stream, response);
  return ok ? 0 : 1;
}

int print_response_json(const http::response& response) {
  if (!response.body.empty()) std::cout << response.body;
  if (!response.body.empty() && response.body.back() != '\n') std::cout << '\n';
  if (response.body.empty() && !(response.status >= 200 && response.status < 300)) {
    print_status_line(std::cerr, response);
  }
  return (response.status >= 200 && response.status < 300) ? 0 : 1;
}

int print_error(const std::string& message) {
  std::cerr << "ERROR: " << message << '\n';
  return 1;
}

}

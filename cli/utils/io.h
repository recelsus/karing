#pragma once

#include <optional>
#include <string>

#include "domain/app_error.h"
#include "http/http_client.h"

namespace karing::cli::utils {

bool stdin_has_data();
std::optional<std::string> read_stdin_text();
int print_response(const http::response& response, bool raw_stdout);
int print_response_json(const http::response& response);
int print_error(const std::string& message);
int print_error(const karing::domain::app_error& error, bool json_output, bool include_detail = false);

}

#pragma once

#include <string>

#include "utils/limits.h"

namespace karing::options {

enum class action {
  run,
  help,
  version,
  error,
};

struct server_options {
  action action_kind{action::run};
  std::string error;
  std::string db_path;
  std::string listen_address{"127.0.0.1"};
  std::string upload_path;
  std::string log_path;
  std::string base_path{"/"};
  bool check_only{false};
  bool init_only{false};
  bool force{false};
  bool show_error_details{false};
  int port{8080};
  int limit{100};
  int max_file_bytes{karing::limits::kDefaultMaxFileMb};
  int max_text_bytes{karing::limits::kDefaultMaxTextMb};
};

server_options parse(int argc, char** argv);
server_options& current();

}  // namespace karing::options

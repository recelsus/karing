#include "utils/options.h"

#include <cstdlib>
#include <string>

#include "utils/limits.h"

namespace karing::options {

namespace {

void parse_int(const char* raw, int& out) {
  if (!raw || !*raw) return;
  try {
    out = std::stoi(raw);
  } catch (...) {
  }
}

}  // namespace

server_options parse(int argc, char** argv) {
  server_options out;

  if (const char* env = std::getenv("KARING_DB_PATH"); env && *env) out.db_path = env;

  parse_int(std::getenv("KARING_PORT"), out.port);

  if (const char* env = std::getenv("KARING_LISTEN"); env && *env) out.listen_address = env;

  parse_int(std::getenv("KARING_LIMIT"), out.limit);
  parse_int(std::getenv("KARING_MAX_FILE"), out.max_file_bytes);
  parse_int(std::getenv("KARING_MAX_TEXT"), out.max_text_bytes);

  if (const char* env = std::getenv("KARING_UPLOAD_PATH"); env && *env) out.upload_path = env;
  if (const char* env = std::getenv("KARING_BASE_PATH"); env && *env) out.base_path = env;

  for (int i = 1; i < argc; ++i) {
    const std::string arg = argv[i];
    if (arg == "-h" || arg == "--help") {
      out.action_kind = action::help;
      return out;
    }
    if (arg == "-v" || arg == "--version") {
      out.action_kind = action::version;
      return out;
    }
    if (arg == "--db-path" && i + 1 < argc) {
      out.db_path = argv[++i];
      continue;
    }
    if (arg == "--force") {
      out.force = true;
      continue;
    }
    if (arg == "--listen" && i + 1 < argc) {
      out.listen_address = argv[++i];
      continue;
    }
    if (arg == "--port" && i + 1 < argc) {
      out.port = std::stoi(argv[++i]);
      continue;
    }
    if (arg == "--limit" && i + 1 < argc) {
      out.limit = std::stoi(argv[++i]);
      continue;
    }
    if (arg == "--max-file" && i + 1 < argc) {
      try {
        out.max_file_bytes = std::stoi(argv[++i]);
      } catch (...) {
      }
      continue;
    }
    if (arg == "--max-text" && i + 1 < argc) {
      try {
        out.max_text_bytes = std::stoi(argv[++i]);
      } catch (...) {
      }
      continue;
    }
    if (arg == "--upload-path" && i + 1 < argc) {
      out.upload_path = argv[++i];
      continue;
    }
    if (arg == "--check-db") {
      out.check_only = true;
      continue;
    }
    if (arg == "--init-db") {
      out.init_only = true;
      continue;
    }
    if (!arg.empty() && arg[0] == '-') {
      out.action_kind = action::error;
      out.error = "unknown option: " + arg;
      return out;
    }
  }

  if (out.check_only && out.init_only) {
    out.action_kind = action::error;
    out.error = "--check-db and --init-db cannot be used together";
  }

  return out;
}

server_options& current() {
  static server_options instance;
  return instance;
}

}  // namespace karing::options

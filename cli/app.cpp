#include "app.h"

#include <cstdlib>
#include <optional>
#include <string>
#include <utility>
#include <vector>

#include "commands/commands.h"
#include "utils/cli_output.h"
#include "utils/io.h"
#include "utils/url.h"

namespace karing::cli {

namespace {

struct dispatch_context {
  std::string base_url;
  std::optional<std::string> api_key;
  bool json_output{false};
  std::vector<std::string> args;
};

dispatch_context parse_global(int argc, char** argv) {
  dispatch_context out;
  for (int i = 1; i < argc; ++i) {
    std::string arg = argv[i];
    if (arg == "--url") {
      if (i + 1 < argc) out.base_url = argv[++i];
      else out.args.push_back(arg);
      continue;
    }
    if (arg == "--api-key") {
      if (i + 1 < argc) out.api_key = argv[++i];
      else out.args.push_back(arg);
      continue;
    }
    if (arg == "--json") {
      out.json_output = true;
      continue;
    }
    out.args.push_back(std::move(arg));
  }
  if (out.base_url.empty()) {
    if (const char* env = std::getenv("KARING_URL"); env && *env) out.base_url = env;
    else if (const char* env = std::getenv("KARING_ENDPOINT"); env && *env) out.base_url = env;
  }
  if (!out.api_key.has_value()) {
    if (const char* env = std::getenv("KARING_API_KEY"); env && *env) out.api_key = std::string(env);
  }
  out.base_url = utils::normalize_base_url(out.base_url);
  return out;
}

bool is_numeric_id_candidate(const std::string& value) {
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

int run(int argc, char** argv) {
  const auto parsed = parse_global(argc, argv);
  if (parsed.args.empty()) {
    if (parsed.base_url.empty()) return utils::print_error("missing server URL; use --url, KARING_URL, or KARING_ENDPOINT");
    return commands::run_get(parsed.base_url, parsed.api_key, std::nullopt, parsed.json_output);
  }

  const auto& first = parsed.args.front();
  if (first == "--help" || first == "-h") {
    utils::print_help();
    return 0;
  }
  if (first == "--version") {
    utils::print_version();
    return 0;
  }
  if (parsed.base_url.empty()) {
    return utils::print_error("missing server URL; use --url, KARING_URL, or KARING_ENDPOINT");
  }

  if (first == "add") return commands::run_add(parsed.base_url, parsed.api_key, parsed.json_output, {parsed.args.begin() + 1, parsed.args.end()});
  if (first == "del") return commands::run_delete(parsed.base_url, parsed.api_key, parsed.json_output, {parsed.args.begin() + 1, parsed.args.end()});
  if (first == "find") return commands::run_find(parsed.base_url, parsed.api_key, parsed.json_output, {parsed.args.begin() + 1, parsed.args.end()});
  if (first == "health") return commands::run_health(parsed.base_url, parsed.api_key, parsed.json_output);
  if (first == "mod") return commands::run_mod(parsed.base_url, parsed.api_key, parsed.json_output, {parsed.args.begin() + 1, parsed.args.end()});
  if (first == "swap") return commands::run_swap(parsed.base_url, parsed.api_key, parsed.json_output, {parsed.args.begin() + 1, parsed.args.end()});

  if (parsed.args.size() == 1 && is_numeric_id_candidate(first)) {
    return commands::run_get(parsed.base_url, parsed.api_key, std::stoi(first), parsed.json_output);
  }

  return commands::run_add(parsed.base_url, parsed.api_key, parsed.json_output, parsed.args);
}

}

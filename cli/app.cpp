#include "app.h"

#include <cstdlib>
#include <optional>
#include <string>
#include <utility>
#include <vector>

#include "commands/commands.h"
#include "utils/arg_utils.h"
#include "utils/cli_output.h"
#include "utils/io.h"
#include "utils/url.h"

namespace karing::cli {

namespace {

struct dispatch_context {
  std::string base_url;
  std::optional<std::string> api_key;
  std::optional<int> explicit_id;
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
    if (arg == "--id") {
      if (i + 1 < argc) {
        try {
          out.explicit_id = std::stoi(argv[++i]);
        } catch (...) {
          out.explicit_id = -1;
        }
      } else {
        out.args.push_back(arg);
      }
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
  }
  if (!out.api_key.has_value()) {
    if (const char* env = std::getenv("KARING_API_KEY"); env && *env) out.api_key = std::string(env);
  }
  out.base_url = utils::normalize_base_url(out.base_url);
  return out;
}

}  // namespace

int run(int argc, char** argv) {
  const auto parsed = parse_global(argc, argv);
  if (parsed.args.empty()) {
    if (parsed.base_url.empty()) return utils::print_error("missing server URL; use --url or KARING_URL");
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
    return utils::print_error("missing server URL; use --url or KARING_URL");
  }
  if (parsed.explicit_id.has_value()) {
    if (*parsed.explicit_id < 1 || *parsed.explicit_id > 1000) {
      return utils::print_error("--id must be in range 1..1000");
    }
    if (!parsed.args.empty()) {
      return utils::print_error("--id cannot be combined with subcommands or free text");
    }
    return commands::run_get(parsed.base_url, parsed.api_key, *parsed.explicit_id, parsed.json_output);
  }

  if (first == "add") return commands::run_add(parsed.base_url, parsed.api_key, parsed.json_output, {parsed.args.begin() + 1, parsed.args.end()});
  if (first == "del") return commands::run_delete(parsed.base_url, parsed.api_key, parsed.json_output, {parsed.args.begin() + 1, parsed.args.end()});
  if (first == "find") return commands::run_find(parsed.base_url, parsed.api_key, parsed.json_output, {parsed.args.begin() + 1, parsed.args.end()});
  if (first == "health") return commands::run_health(parsed.base_url, parsed.api_key, parsed.json_output);
  if (first == "mod") return commands::run_mod(parsed.base_url, parsed.api_key, parsed.json_output, {parsed.args.begin() + 1, parsed.args.end()});
  if (first == "swap") return commands::run_swap(parsed.base_url, parsed.api_key, parsed.json_output, {parsed.args.begin() + 1, parsed.args.end()});

  if (parsed.args.size() == 1 && utils::is_valid_id(first)) {
    return commands::run_get(parsed.base_url, parsed.api_key, std::stoi(first), parsed.json_output);
  }

  return commands::run_add(parsed.base_url, parsed.api_key, parsed.json_output, parsed.args);
}

}

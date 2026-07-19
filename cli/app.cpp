#include "app.h"

#include <cstdlib>
#include <optional>
#include <string>
#include <utility>
#include <vector>

#include "backend/target.h"
#include "backend/sqlite_backend.h"
#if KARING_CLI_ENABLE_HTTP_BACKEND
#include "commands/commands.h"
#endif
#include "utils/arg_utils.h"
#include "utils/cli_output.h"
#include "utils/io.h"

namespace karing::cli {

namespace {

struct dispatch_context {
  std::optional<std::string> api_key;
  std::optional<int> explicit_id;
  bool json_output{false};
  bool show_error_details{false};
  std::string error;
  std::vector<std::string> args;
};

dispatch_context parse_global(int argc, char** argv) {
  dispatch_context out;
  for (int i = 1; i < argc; ++i) {
    std::string arg = argv[i];
    if (arg == "--url") {
      out.error = "--url is no longer supported; pass target as the first argument";
      return out;
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
    if (arg == "--error-detail") {
      out.show_error_details = true;
      continue;
    }
    out.args.push_back(std::move(arg));
  }
  if (!out.api_key.has_value()) {
    if (const char* env = std::getenv("KARING_API_KEY"); env && *env) out.api_key = std::string(env);
  }
  if (const char* env = std::getenv("KARING_ERROR_DETAIL"); env && *env) {
    const std::string value(env);
    out.show_error_details = value == "1" || value == "true" || value == "TRUE" || value == "yes" || value == "on";
  }
  return out;
}

bool is_command_name(const std::string& value) {
  return value == "add" || value == "del" || value == "find" || value == "health" ||
         value == "mod" || value == "swap" || value == "resequence" || value == "init-db";
}

int print_error(const dispatch_context& context, karing::domain::app_error error) {
  return utils::print_error(error, context.json_output, context.show_error_details);
}

int print_validation_error(const dispatch_context& context, const std::string& message) {
  return print_error(context, karing::domain::make_error(karing::domain::error_category::validation,
                                                         karing::domain::error_code::validation,
                                                         message));
}

int run_sqlite_target(const backend::target& target,
                      const dispatch_context& parsed,
                      const std::vector<std::string>& args) {
  const backend::sqlite_context context{
      .db_path = target.value,
      .json_output = parsed.json_output,
      .show_error_details = parsed.show_error_details,
  };
  if (parsed.explicit_id.has_value()) {
    if (*parsed.explicit_id < 1 || *parsed.explicit_id > 1000) {
      return print_validation_error(parsed, "--id must be in range 1..1000");
    }
    if (!args.empty()) return print_validation_error(parsed, "--id cannot be combined with subcommands or free text");
    return backend::run_sqlite_get(context, *parsed.explicit_id);
  }
  if (args.empty()) return backend::run_sqlite_get(context, std::nullopt);
  const auto& first = args.front();
  if (first == "add") return backend::run_sqlite_add(context, {args.begin() + 1, args.end()});
  if (first == "del") return backend::run_sqlite_delete(context, {args.begin() + 1, args.end()});
  if (first == "find") return backend::run_sqlite_find(context, {args.begin() + 1, args.end()});
  if (first == "health") return backend::run_sqlite_health(context);
  if (first == "mod") return backend::run_sqlite_mod(context, {args.begin() + 1, args.end()});
  if (first == "swap") return backend::run_sqlite_swap(context, {args.begin() + 1, args.end()});
  if (first == "resequence") return backend::run_sqlite_resequence(context);
  if (args.size() == 1 && utils::is_valid_id(first)) return backend::run_sqlite_get(context, std::stoi(first));
  return backend::run_sqlite_add(context, args);
}

int run_http_target(const backend::target& target,
                    const dispatch_context& parsed,
                    const std::vector<std::string>& args) {
#if KARING_CLI_ENABLE_HTTP_BACKEND
  const auto& base_url = target.value;
  if (parsed.explicit_id.has_value()) {
    if (*parsed.explicit_id < 1 || *parsed.explicit_id > 1000) {
      return print_validation_error(parsed, "--id must be in range 1..1000");
    }
    if (!args.empty()) {
      return print_validation_error(parsed, "--id cannot be combined with subcommands or free text");
    }
    return commands::run_get(base_url, parsed.api_key, *parsed.explicit_id, parsed.json_output);
  }

  if (args.empty()) return commands::run_get(base_url, parsed.api_key, std::nullopt, parsed.json_output);

  const auto& first = args.front();
  if (first == "add") return commands::run_add(base_url, parsed.api_key, parsed.json_output, {args.begin() + 1, args.end()});
  if (first == "del") return commands::run_delete(base_url, parsed.api_key, parsed.json_output, {args.begin() + 1, args.end()});
  if (first == "find") return commands::run_find(base_url, parsed.api_key, parsed.json_output, {args.begin() + 1, args.end()});
  if (first == "health") return commands::run_health(base_url, parsed.api_key, parsed.json_output);
  if (first == "mod") return commands::run_mod(base_url, parsed.api_key, parsed.json_output, {args.begin() + 1, args.end()});
  if (first == "swap") return commands::run_swap(base_url, parsed.api_key, parsed.json_output, {args.begin() + 1, args.end()});
  if (first == "resequence") return commands::run_resequence(base_url, parsed.api_key, parsed.json_output);

  if (args.size() == 1 && utils::is_valid_id(first)) {
    return commands::run_get(base_url, parsed.api_key, std::stoi(first), parsed.json_output);
  }

  return commands::run_add(base_url, parsed.api_key, parsed.json_output, args);
#else
  (void)target;
  (void)args;
  return print_error(parsed, karing::domain::make_error(karing::domain::error_category::backend,
                                                        karing::domain::error_code::backend_unsupported,
                                                        "HTTP backend is not available in this build"));
#endif
}

}  // namespace

int run(int argc, char** argv) {
  const auto parsed = parse_global(argc, argv);
  if (!parsed.error.empty()) return print_validation_error(parsed, parsed.error);

  if (parsed.args.empty()) return print_validation_error(parsed, "missing target");

  const auto& maybe_action = parsed.args.front();
  if (maybe_action == "--help" || maybe_action == "-h") {
    utils::print_help();
    return 0;
  }
  if (maybe_action == "--version") {
    utils::print_version();
    return 0;
  }
  if (maybe_action == "init-db") {
    return backend::run_sqlite_init_database({parsed.args.begin() + 1, parsed.args.end()}, parsed.json_output, parsed.show_error_details);
  }
  if (is_command_name(maybe_action)) return print_validation_error(parsed, "missing target");

  const auto target = backend::parse_target(maybe_action);
  const std::vector<std::string> command_args(parsed.args.begin() + 1, parsed.args.end());

  if (target.kind == backend::target_kind::http) return run_http_target(target, parsed, command_args);
  return run_sqlite_target(target, parsed, command_args);
}

}

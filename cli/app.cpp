#include "app.h"

#include <cstdlib>
#include <limits>
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
  std::optional<std::string> target;
  std::optional<std::string> api_key;
  bool json_output{false};
  std::string error;
  std::vector<std::string> args;
};

dispatch_context parse_global(int argc, char** argv) {
  dispatch_context out;
  for (int i = 1; i < argc; ++i) {
    std::string arg = argv[i];
    if (arg == "--url") {
      out.error = "--url is no longer supported; use --target";
      return out;
    }
    if (arg == "--api-key") {
      if (i + 1 < argc) out.api_key = argv[++i];
      else out.args.push_back(arg);
      continue;
    }
    if (arg == "--target") {
      if (i + 1 < argc) out.target = argv[++i];
      else {
        out.error = "--target requires a value";
        return out;
      }
      continue;
    }
    if (arg == "--id") {
      out.error = "--id is no longer supported; use get <id>";
      return out;
    }
    if (arg == "--json") {
      out.json_output = true;
      continue;
    }
    if (arg == "--error-detail") {
      out.error = "--error-detail is no longer supported";
      return out;
    }
    out.args.push_back(std::move(arg));
  }
  if (!out.api_key.has_value()) {
    if (const char* env = std::getenv("KARING_API_KEY"); env && *env) out.api_key = std::string(env);
  }
  if (!out.target.has_value()) {
    if (const char* env = std::getenv("KARING_TARGET"); env && *env) out.target = std::string(env);
  }
  return out;
}

bool is_command_name(const std::string& value) {
  return value == "get" || value == "add" || value == "del" || value == "find" ||
         value == "mod" || value == "move" || value == "swap" || value == "resequence" || value == "init-db";
}

bool is_positive_integer(const std::string& value) {
  if (value.empty()) return false;
  for (const char ch : value) {
    if (ch < '0' || ch > '9') return false;
  }
  return true;
}

std::optional<int> parse_positive_id(const std::string& value) {
  if (!is_positive_integer(value)) return std::nullopt;
  try {
    const long long parsed = std::stoll(value);
    if (parsed < 1 || parsed > static_cast<long long>(std::numeric_limits<int>::max())) return std::nullopt;
    return static_cast<int>(parsed);
  } catch (...) {
    return std::nullopt;
  }
}

int print_error(const dispatch_context& context, karing::domain::app_error error) {
  return utils::print_error(error, context.json_output, false);
}

int print_validation_error(const dispatch_context& context, const std::string& message) {
  return print_error(context, karing::domain::make_error(karing::domain::error_category::validation,
                                                         karing::domain::error_code::validation,
                                                         message));
}

int print_not_found_error(const dispatch_context& context, const std::string& message = "not found") {
  return print_error(context, karing::domain::make_error(karing::domain::error_category::not_found,
                                                         karing::domain::error_code::not_found,
                                                         message));
}

std::optional<backend::target> selected_target(const dispatch_context& parsed) {
  if (parsed.target.has_value()) return backend::parse_target(*parsed.target);
  print_validation_error(parsed, "missing target; set KARING_TARGET or pass --target");
  return std::nullopt;
}

int run_sqlite_command(const backend::target& target,
                       const dispatch_context& parsed,
                       const std::string& command,
                       const std::vector<std::string>& args) {
  const backend::sqlite_context context{
      .db_path = target.value,
      .json_output = parsed.json_output,
  };
  if (command == "get") {
    if (args.size() > 1) return print_validation_error(parsed, "get accepts at most one id");
    if (args.empty()) return backend::run_sqlite_get(context, std::nullopt);
    const auto id = parse_positive_id(args.front());
    if (!id.has_value()) return print_validation_error(parsed, "get id must be a positive integer");
    if (*id > 1000) return print_not_found_error(parsed);
    return backend::run_sqlite_get(context, *id);
  }
  if (command == "add") return backend::run_sqlite_add(context, args);
  if (command == "del") return backend::run_sqlite_delete(context, args);
  if (command == "find") return backend::run_sqlite_find(context, args);
  if (command == "mod") return backend::run_sqlite_mod(context, args);
  if (command == "move") return backend::run_sqlite_move(context, args);
  if (command == "swap") return backend::run_sqlite_swap(context, args);
  if (command == "resequence") return backend::run_sqlite_resequence(context);
  utils::print_help(parsed.target);
  return 1;
}

int run_http_command(const backend::target& target,
                     const dispatch_context& parsed,
                     const std::string& command,
                     const std::vector<std::string>& args) {
#if KARING_CLI_ENABLE_HTTP_BACKEND
  const auto& base_url = target.value;
  if (command == "get") {
    if (args.size() > 1) return print_validation_error(parsed, "get accepts at most one id");
    if (args.empty()) return commands::run_get(base_url, parsed.api_key, std::nullopt, parsed.json_output);
    const auto id = parse_positive_id(args.front());
    if (!id.has_value()) return print_validation_error(parsed, "get id must be a positive integer");
    if (*id > 1000) return print_not_found_error(parsed);
    return commands::run_get(base_url, parsed.api_key, *id, parsed.json_output);
  }
  if (command == "add") return commands::run_add(base_url, parsed.api_key, parsed.json_output, args);
  if (command == "del") return commands::run_delete(base_url, parsed.api_key, parsed.json_output, args);
  if (command == "find") return commands::run_find(base_url, parsed.api_key, parsed.json_output, args);
  if (command == "mod") return commands::run_mod(base_url, parsed.api_key, parsed.json_output, args);
  if (command == "move") return commands::run_move(base_url, parsed.api_key, parsed.json_output, args);
  if (command == "swap") return commands::run_swap(base_url, parsed.api_key, parsed.json_output, args);
  if (command == "resequence") return commands::run_resequence(base_url, parsed.api_key, parsed.json_output);
  utils::print_help(parsed.target);
  return 1;
#else
  (void)target;
  (void)command;
  (void)args;
  return print_error(parsed, karing::domain::make_error(karing::domain::error_category::backend,
                                                        karing::domain::error_code::backend_unsupported,
                                                        "HTTP backend is not available in this build"));
#endif
}

int run_target_command(const backend::target& target,
                       const dispatch_context& parsed,
                       const std::string& command,
                       const std::vector<std::string>& args) {
  if (target.kind == backend::target_kind::http) return run_http_command(target, parsed, command, args);
  return run_sqlite_command(target, parsed, command, args);
}

}  // namespace

int run(int argc, char** argv) {
  const auto parsed = parse_global(argc, argv);
  if (!parsed.error.empty()) return print_validation_error(parsed, parsed.error);

  if (parsed.args.empty()) {
    const auto target = selected_target(parsed);
    if (!target.has_value()) return 1;
    return run_target_command(*target, parsed, "get", {});
  }

  const auto& first = parsed.args.front();
  if (first == "--help" || first == "-h") {
    utils::print_help(parsed.target);
    return 0;
  }
  if (first == "--version") {
    utils::print_version();
    return 0;
  }
  if (first == "init-db") {
    return backend::run_sqlite_init_database({parsed.args.begin() + 1, parsed.args.end()}, parsed.json_output);
  }

  if (is_positive_integer(first)) {
    const auto target = selected_target(parsed);
    if (!target.has_value()) return 1;
    return run_target_command(*target, parsed, "get", {first});
  }

  if (is_command_name(first)) {
    const auto target = selected_target(parsed);
    if (!target.has_value()) return 1;
    return run_target_command(*target, parsed, first, {parsed.args.begin() + 1, parsed.args.end()});
  }

  utils::print_help(parsed.target);
  return 1;
}

}

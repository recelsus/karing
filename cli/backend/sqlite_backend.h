#pragma once

#include <optional>
#include <string>
#include <vector>

namespace karing::cli::backend {

struct sqlite_context {
  std::string db_path;
  bool json_output{false};
  bool show_error_details{false};
};

int run_sqlite_get(const sqlite_context& context, std::optional<int> id);
int run_sqlite_add(const sqlite_context& context, const std::vector<std::string>& args);
int run_sqlite_delete(const sqlite_context& context, const std::vector<std::string>& args);
int run_sqlite_find(const sqlite_context& context, const std::vector<std::string>& args);
int run_sqlite_health(const sqlite_context& context);
int run_sqlite_mod(const sqlite_context& context, const std::vector<std::string>& args);
int run_sqlite_swap(const sqlite_context& context, const std::vector<std::string>& args);
int run_sqlite_resequence(const sqlite_context& context);
int run_sqlite_init_database(const std::vector<std::string>& args,
                             bool json_output,
                             bool show_error_details = false);

}  // namespace karing::cli::backend

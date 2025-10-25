#pragma once

#include <optional>
#include <string>

namespace karing::app {

class cli_dispatcher {
public:
  cli_dispatcher(int argc_value, char** argv_value, std::string db_path_value);
  std::optional<int> run() const;

private:
  bool has_flag(const std::string& flag) const;

  bool wants_help() const;
  bool wants_version() const;
  void print_help() const;
  void print_version() const;

  std::optional<std::string> generate_secret_hex() const;
  std::string normalize_ipv4_cidr(const std::string& input) const;

  std::optional<int> handle_new_interface() const;

  int argc_state;
  char** argv_state;
  std::string db_path;
};

}  // namespace karing::app

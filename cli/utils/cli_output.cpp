#include "utils/cli_output.h"

#include <iostream>
#include <optional>
#include <string>

#include "server/version.h"

namespace karing::cli::utils {

void print_help(const std::optional<std::string>& target) {
  std::cout
      << "karing " KARING_VERSION "\n"
      << "Target: ";
  if (target.has_value()) {
    std::cout << *target << "\n";
  } else {
    std::cout << "not set\n";
  }
  std::cout
      << "Usage:\n"
      << "  karing\n"
      << "  karing get [id]\n"
      << "\n"
      << "  karing add <text>\n"
      << "  karing add -f <path> [--mime <type>] [--name <filename>]\n"
      << "\n"
      << "  karing mod <id> <text>\n"
      << "  karing mod <id> -f <path> [--mime <type>] [--name <filename>]\n"
      << "\n"
      << "  karing del\n"
      << "  karing del <id>\n"
      << "\n"
      << "  karing swap <id1> <id2>\n"
      << "  karing move <id> <before-id>\n"
      << "  karing resequence\n"
      << "\n"
      << "  karing find [query]\n"
      << "              [--limit|-l <n>] [--type|-t text|file]\n"
      << "              [--sort|-s id|store|update] [--asc|--desc]\n"
      << "              [--full]\n"
      << "\n"
      << "  karing init-db <path> [--limit|-l <n>] [--force]\n"
      << "\n"
      << "  karing --help\n"
      << "  karing --version\n"
      << "\n"
      << "Options:\n"
      << "  --target <target>\n"
      << "  --api-key <key>\n"
      << "  --json\n";
}

void print_version() {
  std::cout << KARING_VERSION << '\n';
}

}

#include "utils/cli_output.h"

#include <iostream>

#include "server/version.h"

namespace karing::cli::utils {

void print_help() {
  std::cout
      << "karing " KARING_VERSION "\n"
      << "Usage:\n"
      << "  karing [--api-key <key>] [--json] [--error-detail] <target>\n"
      << "  karing [--api-key <key>] [--json] [--error-detail] <target> <id>\n"
      << "  karing [--api-key <key>] [--json] [--error-detail] --id <id> <target>\n"
      << "  karing [--api-key <key>] [--json] [--error-detail] <target> add [text]\n"
      << "  karing [--api-key <key>] [--json] [--error-detail] <target> add -f <path> [--mime <type>] [--name <filename>]\n"
      << "  karing [--api-key <key>] [--json] [--error-detail] <target> mod <id> [text]\n"
      << "  karing [--api-key <key>] [--json] [--error-detail] <target> mod <id> -f <path> [--mime <type>] [--name <filename>]\n"
      << "  karing [--api-key <key>] [--json] [--error-detail] <target> del [id]\n"
      << "  karing [--api-key <key>] [--json] [--error-detail] <target> swap <id1> <id2>\n"
      << "  karing [--api-key <key>] [--json] [--error-detail] <target> resequence\n"
      << "  karing [--api-key <key>] [--json] [--error-detail] <target> find [query] [--limit|-l <n>] [--type|-t text|file]\n"
      << "                                           [--sort|-s id|store|update] [--asc] [--desc] [--full]\n"
      << "  karing [--api-key <key>] [--json] [--error-detail] <target> health\n"
      << "  karing [--json] [--error-detail] init-db <path> [--limit|-l <n>] [--force]\n"
      << "  karing --help\n"
      << "  karing --version\n"
      << "\n"
      << "Target:\n"
#if KARING_CLI_ENABLE_HTTP_BACKEND
      << "  http://... or https://...  HTTP backend\n"
#endif
      << "  any other value            SQLite backend path\n"
      << "API key resolution:\n"
      << "  --api-key > KARING_API_KEY\n"
      << "Error detail:\n"
      << "  --error-detail or KARING_ERROR_DETAIL=1\n";
}

void print_version() {
  std::cout << KARING_VERSION << '\n';
}

}

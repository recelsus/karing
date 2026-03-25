#include "utils/cli_output.h"

#include <iostream>

#include "server/version.h"

namespace karing::cli::utils {

void print_help() {
  std::cout
      << "karing " KARING_VERSION "\n"
      << "Usage:\n"
      << "  karing [--url <url>] [--api-key <key>] [--json]\n"
      << "  karing [--url <url>] [--api-key <key>] [--json] <id>\n"
      << "  karing [--url <url>] [--api-key <key>] [--json] add [text]\n"
      << "  karing [--url <url>] [--api-key <key>] [--json] add -f <path> [--mime <type>] [--name <filename>]\n"
      << "  karing [--url <url>] [--api-key <key>] [--json] mod <id> [text]\n"
      << "  karing [--url <url>] [--api-key <key>] [--json] mod <id> -f <path> [--mime <type>] [--name <filename>]\n"
      << "  karing [--url <url>] [--api-key <key>] [--json] del [id]\n"
      << "  karing [--url <url>] [--api-key <key>] [--json] find [query] [--limit|-l <n>] [--type|-t text|file]\n"
      << "                                  [--sort|-s id|store|update] [--asc] [--desc] [--full]\n"
      << "  karing [--url <url>] [--api-key <key>] [--json] health\n"
      << "  karing --help\n"
      << "  karing --version\n"
      << "\n"
      << "URL resolution:\n"
      << "  --url > KARING_URL > KARING_ENDPOINT\n"
      << "API key resolution:\n"
      << "  --api-key > KARING_API_KEY\n";
}

void print_version() {
  std::cout << KARING_VERSION << '\n';
}

}

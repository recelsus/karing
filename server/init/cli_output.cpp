#include "init/cli_output.h"

#include <iostream>

#include "version.h"

namespace karing::app::init {

void print_help() {
  std::cout
      << "karing " << KARING_VERSION << "\n\n"
      << "Usage:\n"
      << "  karing [server-options]\n\n"
      << "Server options:\n"
      << "  --listen <host>       Override listener address\n"
      << "  --port <n>            Override listener port\n"
      << "  --db-path <path>      Override SQLite database path\n"
      << "  --force               Force shrink by dropping oldest data and reassigning ids\n"
      << "  --max-text <mb>       Override text size cap in MB\n"
      << "  --max-file <mb>       Override file size cap in MB\n"
      << "  --limit <n>           Override active item limit\n"
      << "  --upload-path <path>  Override upload staging path\n"
      << "  --check-db            Check current database schema without modifying it\n"
      << "  --init-db             Initialize or resize database schema then exit\n"
      << "  -h, --help            Show this help message\n"
      << "  -v, --version         Show version info\n";
}

void print_version() {
  std::cout << "karing " << KARING_VERSION << "\n";
}

}  // namespace karing::app::init

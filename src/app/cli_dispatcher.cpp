#include "app/cli_dispatcher.h"

#include <openssl/rand.h>
#include <sqlite3.h>

#include <algorithm>
#include <cstdint>
#include <cstdlib>
#include <iostream>
#include <optional>
#include <sstream>
#include <string>
#include <string_view>
#include <vector>

#include "version.h"

namespace karing::app {

cli_dispatcher::cli_dispatcher(int argc_value, char** argv_value, std::string db_path_value)
    : argc_state(argc_value), argv_state(argv_value), db_path(std::move(db_path_value)) {}

std::optional<int> cli_dispatcher::run() const {
  if (wants_help()) {
    print_help();
    return 0;
  }
  if (wants_version()) {
    print_version();
    return 0;
  }
  if (auto code = handle_new_interface(); code.has_value()) return code;
  return std::nullopt;
}

bool cli_dispatcher::has_flag(const std::string& flag) const {
  for (int i = 1; i < argc_state; ++i) {
    if (std::string_view(argv_state[i]) == flag) return true;
  }
  return false;
}

bool cli_dispatcher::wants_help() const {
  return has_flag("-h") || has_flag("--help");
}

bool cli_dispatcher::wants_version() const {
  return has_flag("-v") || has_flag("--version");
}

void cli_dispatcher::print_help() const {
  std::cout << "karing " << KARING_VERSION << "\n\n"
            << "Usage:\n"
            << "  karing [options]\n"
            << "  karing keys <subcommand> [args]\n"
            << "  karing ip <subcommand> [args]\n\n"
            << "Options:\n"
            << "  -h, --help          Show this help message\n"
            << "  -v, --version       Show version info\n"
            << "  --config <path>     Override config file\n"
            << "  --data <path>       Override SQLite database path\n"
            << "  --port <n>          Override listener port\n"
            << "  --limit <n>         Override active item limit\n"
            << "  --max-file-bytes <n>  Override file size cap\n"
            << "  --max-text-bytes <n>  Override text size cap\n"
            << "  --no-auth           Disable API key auth\n"
            << "  --trusted-proxy     Trust X-Forwarded-For header\n"
            << "  --allow-localhost   Allow unauthenticated localhost reads\n"
            << "  --check-db          Validate database schema then exit\n"
            << "  --base-path <path>  Override base path prefix\n\n"
            << "Commands:\n"
            << "  keys list\n"
            << "  keys add [--role R] [--label L] [--disabled] [--json]\n"
            << "  keys set-role <id> <role>\n"
            << "  keys set-label <id> <label>\n"
            << "  keys disable <id>\n"
            << "  keys enable <id>\n"
            << "  keys rm <id> [--hard]\n"
            << "  ip list [allow|deny]\n"
            << "  ip add <cidr> allow|deny\n"
            << "  ip del allow:<id>|deny:<id>\n\n"
            << std::flush;
}

void cli_dispatcher::print_version() const {
  std::cout << "karing " << KARING_VERSION << "\n"
            << "build " << KARING_BUILD_NUMBER << " (" << KARING_BUILD_TYPE << ")" << "\n"
            << "git " << KARING_GIT_BRANCH << " @ " << KARING_GIT_REV << "\n"
            << "compiled with " << KARING_COMPILER_ID << " " << KARING_COMPILER_VERSION
            << " on " << KARING_BUILD_OS << "\n";
}

std::optional<std::string> cli_dispatcher::generate_secret_hex() const {
  unsigned char buffer[24];
  if (RAND_bytes(buffer, sizeof(buffer)) != 1) return std::nullopt;
  static const char* hex = "0123456789abcdef";
  std::string secret;
  secret.resize(sizeof(buffer) * 2);
  for (size_t i = 0; i < sizeof(buffer); ++i) {
    secret[i * 2] = hex[(buffer[i] >> 4) & 0xF];
    secret[i * 2 + 1] = hex[buffer[i] & 0xF];
  }
  return secret;
}

std::string cli_dispatcher::normalize_ipv4_cidr(const std::string& input) const {
  auto slash = input.find('/');
  std::string ip_part = input.substr(0, slash);
  int prefix_bits = -1;
  if (slash != std::string::npos) {
    try {
      prefix_bits = std::stoi(input.substr(slash + 1));
    } catch (...) {
      prefix_bits = -1;
    }
    if (prefix_bits < 0 || prefix_bits > 32) return input;
  }
  int octets[4] = {0, 0, 0, 0};
  char dummy;
  std::istringstream stream(ip_part);
  if (!(stream >> octets[0] >> dummy >> octets[1] >> dummy >> octets[2] >> dummy >> octets[3])) return input;
  for (int& octet : octets) {
    if (octet < 0 || octet > 255) return input;
  }
  if (prefix_bits < 0) return input;
  uint32_t numeric = (static_cast<uint32_t>(octets[0]) << 24)
                   | (static_cast<uint32_t>(octets[1]) << 16)
                   | (static_cast<uint32_t>(octets[2]) << 8)
                   | (static_cast<uint32_t>(octets[3]));
  uint32_t mask = (prefix_bits == 0) ? 0u : 0xFFFFFFFFu << (32 - prefix_bits);
  uint32_t network = numeric & mask;
  std::ostringstream oss;
  oss << ((network >> 24) & 0xFF) << '.'
      << ((network >> 16) & 0xFF) << '.'
      << ((network >> 8) & 0xFF) << '.'
      << (network & 0xFF) << '/' << prefix_bits;
  return oss.str();
}

std::optional<int> cli_dispatcher::handle_new_interface() const {
  if (argc_state < 2) return std::nullopt;
  std::string cmd1 = argv_state[1];
  if (cmd1 == "keys") {
    if (argc_state < 3) return std::nullopt;
    std::string cmd2 = argv_state[2];
    if (cmd2 == "list" || cmd2 == "ls") {
      sqlite3* db_handle = nullptr;
      if (sqlite3_open_v2(db_path.c_str(), &db_handle, SQLITE_OPEN_READONLY, nullptr) != SQLITE_OK) return 1;
      const char* query =
          "SELECT id, key, label, role, enabled, created_at, last_used_at, last_ip FROM api_keys ORDER BY id;";
      sqlite3_stmt* stmt = nullptr;
      if (sqlite3_prepare_v2(db_handle, query, -1, &stmt, nullptr) == SQLITE_OK) {
        std::cout << "id\tkey\tlabel\trole\tenabled\tcreated_at\tlast_used_at\tlast_ip\n";
        while (sqlite3_step(stmt) == SQLITE_ROW) {
          const unsigned char* key_text = sqlite3_column_text(stmt, 1);
          const unsigned char* label_text = sqlite3_column_text(stmt, 2);
          const unsigned char* role_text = sqlite3_column_text(stmt, 3);
          const unsigned char* ip_text = sqlite3_column_text(stmt, 7);
          std::string last_used = (sqlite3_column_type(stmt, 6) != SQLITE_NULL)
                                      ? std::to_string(sqlite3_column_int64(stmt, 6))
                                      : std::string();
          std::cout << sqlite3_column_int(stmt, 0) << '\t'
                    << (key_text ? reinterpret_cast<const char*>(key_text) : "") << '\t'
                    << (label_text ? reinterpret_cast<const char*>(label_text) : "") << '\t'
                    << (role_text ? reinterpret_cast<const char*>(role_text) : "") << '\t'
                    << sqlite3_column_int(stmt, 4) << '\t'
                    << sqlite3_column_int64(stmt, 5) << '\t'
                    << last_used << '\t'
                    << (ip_text ? reinterpret_cast<const char*>(ip_text) : "")
                    << '\n';
        }
      } else {
        std::cout << "prepare failed: " << sqlite3_errmsg(db_handle) << '\n';
      }
      if (stmt) sqlite3_finalize(stmt);
      sqlite3_close(db_handle);
      return 0;
    }
    if (cmd2 == "add") {
      bool want_json = false;
      bool want_disabled = false;
      std::string role = "write";
      std::string label;
      for (int i = 3; i < argc_state; ++i) {
        std::string arg = argv_state[i];
        if (arg == "--json") want_json = true;
        else if (arg == "--disabled") want_disabled = true;
        else if (arg == "--role" && i + 1 < argc_state) role = argv_state[++i];
        else if (arg == "--label" && i + 1 < argc_state) label = argv_state[++i];
      }
      auto maybe_secret = generate_secret_hex();
      if (!maybe_secret) {
        std::cerr << "random generation failed\n";
        return 1;
      }
      sqlite3* db_handle = nullptr;
      if (sqlite3_open_v2(db_path.c_str(), &db_handle, SQLITE_OPEN_READWRITE, nullptr) != SQLITE_OK) return 1;
      sqlite3_stmt* stmt = nullptr;
      const char* insert_sql = "INSERT INTO api_keys(key,label,enabled,role,created_at) VALUES(?,?,?,?,strftime('%s','now'));";
      if (sqlite3_prepare_v2(db_handle, insert_sql, -1, &stmt, nullptr) == SQLITE_OK) {
        sqlite3_bind_text(stmt, 1, maybe_secret->c_str(), -1, SQLITE_TRANSIENT);
        if (!label.empty()) sqlite3_bind_text(stmt, 2, label.c_str(), -1, SQLITE_TRANSIENT);
        else sqlite3_bind_null(stmt, 2);
        sqlite3_bind_int(stmt, 3, want_disabled ? 0 : 1);
        sqlite3_bind_text(stmt, 4, role.c_str(), -1, SQLITE_TRANSIENT);
        sqlite3_step(stmt);
      }
      if (stmt) sqlite3_finalize(stmt);
      long long new_id = sqlite3_last_insert_rowid(db_handle);
      sqlite3_close(db_handle);
      if (want_json) {
        std::cout << "{\"id\":" << new_id
                  << ",\"role\":\"" << role << "\""
                  << ",\"label\":\"" << label << "\""
                  << ",\"enabled\":" << (want_disabled ? 0 : 1)
                  << ",\"secret\":\"" << *maybe_secret << "\"}" << std::endl;
      } else {
        std::cout << *maybe_secret << std::endl;
      }
      return 0;
    }
    if (cmd2 == "set-role" && argc_state >= 5) {
      int id = std::atoi(argv_state[3]);
      std::string role = argv_state[4];
      sqlite3* db_handle = nullptr;
      if (sqlite3_open_v2(db_path.c_str(), &db_handle, SQLITE_OPEN_READWRITE, nullptr) != SQLITE_OK) return 1;
      sqlite3_stmt* stmt = nullptr;
      const char* update_sql = "UPDATE api_keys SET role=? WHERE id=?;";
      if (sqlite3_prepare_v2(db_handle, update_sql, -1, &stmt, nullptr) == SQLITE_OK) {
        sqlite3_bind_text(stmt, 1, role.c_str(), -1, SQLITE_TRANSIENT);
        sqlite3_bind_int(stmt, 2, id);
        sqlite3_step(stmt);
      }
      if (stmt) sqlite3_finalize(stmt);
      sqlite3_close(db_handle);
      std::cout << "role updated\n";
      return 0;
    }
    if (cmd2 == "set-label" && argc_state >= 5) {
      int id = std::atoi(argv_state[3]);
      std::string label = argv_state[4];
      sqlite3* db_handle = nullptr;
      if (sqlite3_open_v2(db_path.c_str(), &db_handle, SQLITE_OPEN_READWRITE, nullptr) != SQLITE_OK) return 1;
      sqlite3_stmt* stmt = nullptr;
      const char* update_sql = "UPDATE api_keys SET label=? WHERE id=?;";
      if (sqlite3_prepare_v2(db_handle, update_sql, -1, &stmt, nullptr) == SQLITE_OK) {
        if (!label.empty()) sqlite3_bind_text(stmt, 1, label.c_str(), -1, SQLITE_TRANSIENT);
        else sqlite3_bind_null(stmt, 1);
        sqlite3_bind_int(stmt, 2, id);
        sqlite3_step(stmt);
      }
      if (stmt) sqlite3_finalize(stmt);
      sqlite3_close(db_handle);
      std::cout << "label updated\n";
      return 0;
    }
    if (cmd2 == "disable" && argc_state >= 4) {
      int id = std::atoi(argv_state[3]);
      sqlite3* db_handle = nullptr;
      if (sqlite3_open_v2(db_path.c_str(), &db_handle, SQLITE_OPEN_READWRITE, nullptr) != SQLITE_OK) return 1;
      sqlite3_stmt* stmt = nullptr;
      const char* update_sql = "UPDATE api_keys SET enabled=0 WHERE id=?;";
      if (sqlite3_prepare_v2(db_handle, update_sql, -1, &stmt, nullptr) == SQLITE_OK) {
        sqlite3_bind_int(stmt, 1, id);
        sqlite3_step(stmt);
      }
      if (stmt) sqlite3_finalize(stmt);
      sqlite3_close(db_handle);
      std::cout << "disabled\n";
      return 0;
    }
    if (cmd2 == "enable" && argc_state >= 4) {
      int id = std::atoi(argv_state[3]);
      sqlite3* db_handle = nullptr;
      if (sqlite3_open_v2(db_path.c_str(), &db_handle, SQLITE_OPEN_READWRITE, nullptr) != SQLITE_OK) return 1;
      sqlite3_stmt* stmt = nullptr;
      const char* update_sql = "UPDATE api_keys SET enabled=1 WHERE id=?;";
      if (sqlite3_prepare_v2(db_handle, update_sql, -1, &stmt, nullptr) == SQLITE_OK) {
        sqlite3_bind_int(stmt, 1, id);
        sqlite3_step(stmt);
      }
      if (stmt) sqlite3_finalize(stmt);
      sqlite3_close(db_handle);
      std::cout << "enabled\n";
      return 0;
    }
    if (cmd2 == "rm" && argc_state >= 4) {
      int id = std::atoi(argv_state[3]);
      bool hard = false;
      for (int i = 4; i < argc_state; ++i) {
        if (std::string_view(argv_state[i]) == "--hard") hard = true;
      }
      sqlite3* db_handle = nullptr;
      if (sqlite3_open_v2(db_path.c_str(), &db_handle, SQLITE_OPEN_READWRITE, nullptr) != SQLITE_OK) return 1;
      sqlite3_stmt* stmt = nullptr;
      const char* sql = hard ? "DELETE FROM api_keys WHERE id=?;" : "UPDATE api_keys SET enabled=0 WHERE id=?;";
      if (sqlite3_prepare_v2(db_handle, sql, -1, &stmt, nullptr) == SQLITE_OK) {
        sqlite3_bind_int(stmt, 1, id);
        sqlite3_step(stmt);
      }
      if (stmt) sqlite3_finalize(stmt);
      sqlite3_close(db_handle);
      std::cout << (hard ? "deleted\n" : "disabled\n");
      return 0;
    }
  } else if (cmd1 == "ip") {
    if (argc_state < 3) return std::nullopt;
    std::string cmd2 = argv_state[2];
    if (cmd2 == "list" || cmd2 == "ls") {
      std::string target;
      for (int i = 3; i < argc_state; ++i) {
        std::string arg = argv_state[i];
        if (arg == "allow" || arg == "deny") {
          target = arg;
          break;
        }
      }
      bool show_allow = target.empty() || target == "allow";
      bool show_deny = target.empty() || target == "deny";
      if (!show_allow && !show_deny) {
        std::cerr << "ip list must be allow|deny\n";
        return 1;
      }
      sqlite3* db_handle = nullptr;
      if (sqlite3_open_v2(db_path.c_str(), &db_handle, SQLITE_OPEN_READONLY, nullptr) != SQLITE_OK) return 1;
      auto dump = [&](const char* table) {
        std::string sql = std::string("SELECT id, cidr, enabled, created_at FROM ") + table + " ORDER BY id;";
        sqlite3_stmt* stmt = nullptr;
        if (sqlite3_prepare_v2(db_handle, sql.c_str(), -1, &stmt, nullptr) == SQLITE_OK) {
          std::cout << table << '\n';
          std::cout << "id\tcidr\tenabled\tcreated_at\n";
          while (sqlite3_step(stmt) == SQLITE_ROW) {
            const unsigned char* cidr = sqlite3_column_text(stmt, 1);
            std::cout << sqlite3_column_int(stmt, 0) << '\t'
                      << (cidr ? reinterpret_cast<const char*>(cidr) : "") << '\t'
                      << sqlite3_column_int(stmt, 2) << '\t'
                      << sqlite3_column_int64(stmt, 3) << '\n';
          }
        }
        if (stmt) sqlite3_finalize(stmt);
      };
      if (show_allow) dump("ip_allow");
      if (show_deny) dump("ip_deny");
      sqlite3_close(db_handle);
      return 0;
    }
    if (cmd2 == "add" && argc_state >= 5) {
      std::string raw = argv_state[3];
      std::string list = argv_state[4];
      std::string to_store = normalize_ipv4_cidr(raw);
      const char* table = (list == "allow") ? "ip_allow" : (list == "deny" ? "ip_deny" : nullptr);
      if (!table) {
        std::cerr << "ip list must be allow|deny\n";
        return 1;
      }
      sqlite3* db_handle = nullptr;
      if (sqlite3_open_v2(db_path.c_str(), &db_handle, SQLITE_OPEN_READWRITE, nullptr) != SQLITE_OK) return 1;
      sqlite3_stmt* stmt = nullptr;
      std::string insert_sql = std::string("INSERT OR IGNORE INTO ") + table + "(cidr, enabled, created_at) VALUES(?,1,strftime('%s','now'));";
      if (sqlite3_prepare_v2(db_handle, insert_sql.c_str(), -1, &stmt, nullptr) == SQLITE_OK) {
        sqlite3_bind_text(stmt, 1, to_store.c_str(), -1, SQLITE_TRANSIENT);
        sqlite3_step(stmt);
      }
      if (stmt) sqlite3_finalize(stmt);
      sqlite3_close(db_handle);
      std::cout << "ok\n";
      return 0;
    }
    if (cmd2 == "del" && argc_state >= 4) {
      std::string spec = argv_state[3];
      auto pos = spec.find(':');
      if (pos == std::string::npos) {
        std::cerr << "use allow:<id> or deny:<id>\n";
        return 1;
      }
      std::string list = spec.substr(0, pos);
      int id = std::atoi(spec.substr(pos + 1).c_str());
      const char* table = (list == "allow") ? "ip_allow" : (list == "deny" ? "ip_deny" : nullptr);
      if (!table) {
        std::cerr << "ip list must be allow|deny\n";
        return 1;
      }
      sqlite3* db_handle = nullptr;
      if (sqlite3_open_v2(db_path.c_str(), &db_handle, SQLITE_OPEN_READWRITE, nullptr) != SQLITE_OK) return 1;
      sqlite3_stmt* stmt = nullptr;
      std::string delete_sql = std::string("DELETE FROM ") + table + " WHERE id=?;";
      if (sqlite3_prepare_v2(db_handle, delete_sql.c_str(), -1, &stmt, nullptr) == SQLITE_OK) {
        sqlite3_bind_int(stmt, 1, id);
        sqlite3_step(stmt);
      }
      if (stmt) sqlite3_finalize(stmt);
      sqlite3_close(db_handle);
      std::cout << "ok\n";
      return 0;
    }
  }
  return std::nullopt;
}

}  // namespace karing::app

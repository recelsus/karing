#include "db_path.h"

#include <filesystem>
#include <fstream>
#include <string>

namespace fs = std::filesystem;

namespace karing::db {

namespace {

constexpr const char* kDefaultDbPath = "/var/lib/karing/karing.sqlite";

bool touch_db_file(const fs::path& db_path, std::string& error) {
  std::error_code ec;
  const auto parent = db_path.parent_path();
  if (!parent.empty()) {
    fs::create_directories(parent, ec);
    if (ec) {
      error = ec.message();
      return false;
    }
  }

  if (!fs::exists(db_path)) {
    std::ofstream file(db_path);
    if (!file.is_open()) {
      error = "failed to create sqlite file";
      return false;
    }
  }

  return true;
}

fs::path fallback_data_dir() {
  if (const char* xdg = std::getenv("XDG_DATA_HOME"); xdg && *xdg) {
    return fs::path(xdg) / "karing";
  }
  if (const char* home = std::getenv("HOME"); home && *home) {
    return fs::path(home) / ".local" / "share" / "karing";
  }
  return fs::current_path() / ".local" / "share" / "karing";
}

}  // namespace

db_path_result resolve_db_path(const std::string& requested_path, bool create_if_missing) {
  db_path_result result;

  if (!requested_path.empty()) {
    const fs::path explicit_path = fs::absolute(requested_path);
    if (!create_if_missing) {
      if (!fs::exists(explicit_path)) {
        result.error = "database file does not exist";
        return result;
      }
    } else if (!touch_db_file(explicit_path, result.error)) {
      return result;
    }
    result.ok = true;
    result.path = fs::weakly_canonical(explicit_path).string();
    return result;
  }

  const fs::path default_path = kDefaultDbPath;
  const fs::path fallback_path = fallback_data_dir() / "karing.sqlite";

  if (fs::exists(default_path)) {
    result.ok = true;
    result.path = fs::weakly_canonical(default_path).string();
    return result;
  }

  if (fs::exists(fallback_path)) {
    result.ok = true;
    result.used_fallback = true;
    result.path = fs::weakly_canonical(fallback_path).string();
    return result;
  }

  if (create_if_missing) {
    std::string default_error;
    if (touch_db_file(default_path, default_error)) {
      result.ok = true;
      result.path = fs::weakly_canonical(default_path).string();
      return result;
    }

    std::string fallback_error;
    if (touch_db_file(fallback_path, fallback_error)) {
      result.ok = true;
      result.used_fallback = true;
      result.path = fs::weakly_canonical(fallback_path).string();
      return result;
    }

    result.error = "default path '" + default_path.string() + "' failed: " + default_error +
                   "; fallback path '" + fallback_path.string() + "' failed: " + fallback_error;
    return result;
  }

  result.error = "database file does not exist";
  return result;
}

}  // namespace karing::db

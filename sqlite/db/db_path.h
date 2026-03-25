#pragma once

#include <string>

namespace karing::db {

struct db_path_result {
  bool ok{false};
  bool used_fallback{false};
  std::string path;
  std::string error;
};

db_path_result resolve_db_path(const std::string& requested_path, bool create_if_missing = true);

}  // namespace karing::db

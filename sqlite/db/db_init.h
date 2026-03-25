#pragma once
#include <string>

namespace karing::db {

struct init_result {
  bool ok{false};
  bool created{false};
  bool resized{false};
  int previous_max_items{0};
  int current_max_items{0};
  std::string error;
};

// Create or resize the SQLite schema to match the requested max_items.
init_result init_sqlite_schema_file(const std::string& db_path, int max_items, bool force);

}

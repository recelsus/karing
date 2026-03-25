#pragma once
#include <string>
#include <optional>
#include <map>
#include <vector>
#include <utility>

namespace karing::db::inspect {

struct health_info {
  int max_items{};
  int next_id{};
  int active_items{};
};

struct schema_check_result {
  bool ok{false};
  std::string error;
};

// Returns list of (table_name, create_sql)
std::vector<std::pair<std::string, std::string>> list_tables_with_sql(const std::string& db_path);
std::optional<health_info> read_health_info(const std::string& db_path);
schema_check_result check_schema(const std::string& db_path);

}

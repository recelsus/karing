#pragma once
#include <string>
#include <vector>
#include <utility>

namespace karing::db::inspect {

// Returns list of (table_name, create_sql)
std::vector<std::pair<std::string, std::string>> list_tables_with_sql(const std::string& db_path);

}


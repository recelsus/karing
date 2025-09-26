#pragma once
#include <string>
#include <drogon/orm/DbClient.h>

namespace karing::db {

// Ensure the SQLite DB file and parent directory exist. Returns resolved path.
std::string ensure_db_path(const std::string& db_path);

// Apply PRAGMAs and create tables using raw SQLite C API for the given file.
void init_sqlite_schema_file(const std::string& db_path);

}

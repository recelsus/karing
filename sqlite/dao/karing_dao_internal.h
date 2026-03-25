#pragma once

#include <cstdint>
#include <optional>
#include <string>
#include <vector>

#include <sqlite3.h>

#include "karing_dao.h"

namespace karing::dao::detail {

struct Db {
  sqlite3* handle{nullptr};
  explicit Db(const std::string& path);
  ~Db();
  operator sqlite3*();
  bool ok() const;
};

int64_t now_epoch();
const char* sort_column(SortField sort);
std::string qualified_sort_column(SortField sort, const char* table_alias);
std::string order_by_clause(SortField sort, bool desc, const char* table_alias = nullptr);
std::string media_kind_for_mime(const std::string& mime);

bool exec_simple(sqlite3* db, const char* sql);
bool read_entry_file_path(sqlite3* db, int id, std::string& file_path);
void remove_file_if_any(const std::string& path);
bool write_file_for_slot(const std::string& upload_root, int id, const std::string& data, std::string& out_path);
bool fetch_slot_state(sqlite3* db, int& id, int& max_items);
std::optional<int> previous_slot_id(sqlite3* db);
bool advance_next_id(sqlite3* db, int max_items);
bool load_entry(sqlite3* db, int id, KaringRecord& record, std::string* file_path = nullptr, bool require_used = true);

}  // namespace karing::dao::detail

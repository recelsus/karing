#pragma once

#include <string>
#include <vector>

#include <sqlite3.h>

#include "db_init.h"

namespace karing::db::detail {

constexpr int kSchemaVersion = 2;

struct active_entry {
  bool has_source_kind{false};
  std::string source_kind;
  bool has_media_kind{false};
  std::string media_kind;
  bool has_content_text{false};
  std::string content_text;
  bool has_file_path{false};
  std::string file_path;
  bool has_original_filename{false};
  std::string original_filename;
  bool has_mime_type{false};
  std::string mime_type;
  int size_bytes{0};
  long long stored_at{0};
  long long updated_at{0};
};

bool exec_sql(sqlite3* db, const std::string& sql, std::string& error);
bool exec_stmt(sqlite3* db, const char* sql, std::string& error);
bool has_table(sqlite3* db, const char* table_name, std::string& error);
bool seed_metadata(sqlite3* db, std::string& error);
bool ensure_store_state(sqlite3* db, int max_items, bool& created, int& previous_max_items, std::string& error);
bool ensure_slots(sqlite3* db, int start_id, int end_id, std::string& error);
bool update_store_state(sqlite3* db, int max_items, bool reset_next_id, std::string& error);
bool drop_fts_objects(sqlite3* db, std::string& error);
bool rebuild_fts(sqlite3* db, std::string& error);

std::string column_text(sqlite3_stmt* stmt, int index);
bool load_active_entries(sqlite3* db, std::vector<active_entry>& entries, std::string& error);
bool repopulate_active_entries(sqlite3* db, const std::vector<active_entry>& entries, std::string& error);
void remove_files(const std::vector<std::string>& paths);
bool shrink_slots(sqlite3* db, int new_max_items, bool force, std::vector<std::string>& files_to_remove, std::string& error);

bool prepare_schema(sqlite3* db, int max_items, init_result& result, std::string& error);
bool apply_resize(sqlite3* db, int requested_max_items, bool force, init_result& result, std::vector<std::string>& files_to_remove, bool& reset_next_id, std::string& error);
bool finalize_schema(sqlite3* db, int current_max_items, bool reset_next_id, std::string& error);

}  // namespace karing::db::detail

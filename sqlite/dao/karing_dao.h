#pragma once
#include <string>
#include <vector>
#include <optional>

namespace karing::dao {

enum class SortField {
  id,
  stored_at,
  updated_at,
};

struct KaringRecord {
  int id{};
  bool is_file{};
  std::string content; // for text
  std::string filename;
  std::string mime;
  int64_t created_at{};
  std::optional<int64_t> updated_at;
};

class KaringDao {
 public:
  KaringDao(std::string db_path, std::string upload_path);


  // Insert or rotate a text record. Returns row id or <0 on error.
  int insert_text(const std::string& content);

  // Logical delete: set is_active=0 and clear payload.
  bool logical_delete(int id);
  bool logical_delete_latest_recent(int max_age_seconds);

  // Fetch latest active record id.
  std::optional<int> latest_id();

  // Insert file blob.
  int insert_file(const std::string& filename,
                  const std::string& mime,
                  const std::string& data);

  // Fetch single by id.
  std::optional<KaringRecord> get_by_id(int id);
  // Fetch file blob by id (active + is_file=1).
  bool get_file_blob(int id, std::string& out_mime, std::string& out_filename, std::string& out_data);

  // List latest active up to limit.
  std::vector<KaringRecord> list_latest(int limit, SortField sort, bool desc);

  bool try_search_fts(const std::string& fts_query, int limit, SortField sort, bool desc, std::vector<KaringRecord>& out);
  // Count active rows.
  int count_active();

  // Counts for search result sets
  bool count_search_fts(const std::string& fts_query, long long& out);

  struct Filters {
    bool include_inactive{false};
    std::optional<int> is_file; // 0 or 1
    std::optional<std::string> mime;
    std::optional<std::string> filename;
    SortField sort{SortField::stored_at};
    bool order_desc{true};
  };

  std::vector<KaringRecord> list_filtered(int limit, const Filters& f);
  long long count_filtered(const Filters& f);

  // Replace (PUT) operations
  bool update_text(int id, const std::string& content);
  bool update_file(int id, const std::string& filename, const std::string& mime, const std::string& data);

  // Patch (partial) operations
  bool patch_text(int id, const std::optional<std::string>& content);
  bool patch_file(int id, const std::optional<std::string>& filename, const std::optional<std::string>& mime, const std::optional<std::string>& data);

  // Swap the full contents of two slots atomically.
  bool swap_entries(int id1, int id2);

 private:
  std::string db_path_;
  std::string upload_path_;
};

}

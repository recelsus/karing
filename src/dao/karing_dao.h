#pragma once
#include <string>
#include <vector>
#include <optional>

namespace karing::dao {

struct KaringRecord {
  int id{};
  bool is_file{};
  std::string key;
  std::string content; // for text
  std::string filename;
  std::string mime;
  int64_t created_at{};
  std::optional<int64_t> updated_at;
};

class KaringDao {
 public:
  explicit KaringDao(std::string db_path);

  // Ensure there are at least n rows allocated (inactive slots).
  bool preallocate_slots(int n);

  // Insert or rotate a text record. Returns row id or <0 on error.
  int insert_text(const std::string& content,
                  const std::string& syntax,
                  const std::string& key = std::string(),
                  const std::string& client_ip = std::string(),
                  const std::optional<int>& api_key_id = std::nullopt);

  // Logical delete: set is_active=0 and clear payload.
  bool logical_delete(int id);

  // Fetch latest active record id.
  std::optional<int> latest_id();

  // Insert file blob.
  int insert_file(const std::string& filename,
                  const std::string& mime,
                  const std::string& data,
                  const std::string& key = std::string(),
                  const std::string& client_ip = std::string(),
                  const std::optional<int>& api_key_id = std::nullopt);

  // Fetch single by id.
  std::optional<KaringRecord> get_by_id(int id);
  // Fetch file blob by id (active + is_file=1).
  bool get_file_blob(int id, std::string& out_mime, std::string& out_filename, std::string& out_data);

  // List latest active up to limit, with offset.
  std::vector<KaringRecord> list_latest(int limit, int offset = 0);

  // Full-text search over text content (active, non-file only).
  std::vector<KaringRecord> search_text(const std::string& term, int limit, int offset);
  bool try_search_fts(const std::string& fts_query, int limit, int offset, std::vector<KaringRecord>& out);
  std::vector<KaringRecord> search_text_like(const std::string& needle, int limit, int offset);

  // Count active rows.
  int count_active();

  // Counts for search result sets
  bool count_search_fts(const std::string& fts_query, long long& out);
  long long count_search_like(const std::string& needle);

  // Cursor helpers
  std::vector<KaringRecord> list_latest_after(int limit, long long cursor_ts, int cursor_id);
  bool try_search_fts_after(const std::string& fts_query, int limit, long long cursor_ts, int cursor_id, std::vector<KaringRecord>& out);
  std::vector<KaringRecord> search_text_like_after(const std::string& needle, int limit, long long cursor_ts, int cursor_id);

  struct Filters {
    bool include_inactive{false};
    std::optional<std::string> key;
    std::optional<int> is_file; // 0 or 1
    std::optional<std::string> mime;
    std::optional<std::string> filename;
    bool order_desc{true};
  };

  std::vector<KaringRecord> list_filtered(int limit, int offset, const Filters& f);
  long long count_filtered(const Filters& f);

  // Replace (PUT) operations
  bool update_text(int id, const std::string& content, const std::string& syntax,
                   const std::string& client_ip = std::string(),
                   const std::optional<int>& api_key_id = std::nullopt);
  bool update_file(int id, const std::string& filename, const std::string& mime, const std::string& data,
                   const std::string& client_ip = std::string(),
                   const std::optional<int>& api_key_id = std::nullopt);

  // Patch (partial) operations
  bool patch_text(int id, const std::optional<std::string>& content, const std::optional<std::string>& syntax,
                  const std::string& client_ip = std::string(),
                  const std::optional<int>& api_key_id = std::nullopt);
  bool patch_file(int id, const std::optional<std::string>& filename, const std::optional<std::string>& mime, const std::optional<std::string>& data,
                  const std::string& client_ip = std::string(),
                  const std::optional<int>& api_key_id = std::nullopt);

  // Restore latest snapshot (text only) from overwrite_log; returns true on success.
  bool restore_latest_snapshot(int id);

 private:
  std::string db_path_;
};

}

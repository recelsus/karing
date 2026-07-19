#pragma once

#include <optional>
#include <string>
#include <utility>
#include <vector>

#include "dao/karing_dao.h"

namespace karing::domain {

using EntryRecord = karing::dao::KaringRecord;

enum class operation {
  create_text,
  create_file,
  read_record,
  read_file_blob,
  update_text,
  update_file,
  delete_record,
  search,
  swap,
  resequence,
};

struct operation_capabilities {
  bool can_create_text{true};
  bool can_create_file{true};
  bool can_read_record{true};
  bool can_read_file_blob{true};
  bool can_update_text{true};
  bool can_update_file{true};
  bool can_delete_record{true};
  bool can_search{true};
  bool can_swap{true};
  bool can_resequence{true};
  bool can_read_file_metadata{true};
};

operation_capabilities server_capabilities();
operation_capabilities sqlite_cli_capabilities();
bool is_operation_supported(operation_capabilities capabilities, operation op);

struct file_blob {
  std::string mime;
  std::string filename;
  std::string data;
};

enum class search_error {
  none,
  missing_query,
  invalid_sort,
  invalid_order,
  invalid_query,
  fts_unavailable,
};

struct search_request {
  std::string q;
  int limit{0};
  std::string type;
  std::string sort;
  std::string order;
};

struct search_result {
  search_error error{search_error::none};
  std::optional<std::string> detail_reason;
  std::vector<EntryRecord> records;
  long long total{0};
  bool has_total{false};
  int limit{0};
  std::string sort{"id"};
  std::string order{"desc"};
  bool live{false};
};

class entry_operations {
 public:
  entry_operations(std::string db_path, std::string upload_path, int max_limit = 100);

  std::optional<EntryRecord> latest_record() const;
  std::optional<EntryRecord> record_by_id(int id) const;
  bool file_blob_by_id(int id, file_blob& out) const;

  int create_text(const std::string& content) const;
  int create_file(const std::string& filename, const std::string& mime, const std::string& data) const;

  bool replace_text(int id, const std::string& content) const;
  bool replace_file(int id, const std::string& filename, const std::string& mime, const std::string& data) const;

  bool patch_text(int id, const std::optional<std::string>& content) const;
  bool patch_file(int id,
                  const std::optional<std::string>& filename,
                  const std::optional<std::string>& mime,
                  const std::optional<std::string>& data) const;

  bool delete_latest_recent(int max_age_seconds) const;
  bool delete_by_id(int id) const;

  std::optional<std::pair<EntryRecord, EntryRecord>> swap(int id1, int id2) const;
  std::optional<std::pair<std::vector<EntryRecord>, int>> resequence() const;

  search_result search(const search_request& request) const;
  search_result live_search(const search_request& request) const;

 private:
  karing::dao::KaringDao make_dao() const;

  std::string db_path_;
  std::string upload_path_;
  int max_limit_{100};
};

}  // namespace karing::domain

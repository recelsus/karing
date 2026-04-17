#pragma once

#include <optional>
#include <string>
#include <vector>

#include "dao/karing_dao.h"

namespace karing::repository {

class entry_repository {
 public:
  explicit entry_repository(std::string db_path);

  std::optional<int> latest_id() const;
  std::optional<karing::dao::KaringRecord> get_by_id(int id) const;
  bool get_file_record(int id, karing::dao::KaringRecord& record, std::string& file_path) const;

  std::vector<karing::dao::KaringRecord> list_latest(int limit, karing::dao::SortField sort, bool desc) const;
  bool search_fts(const std::string& fts_query,
                  int limit,
                  karing::dao::SortField sort,
                  bool desc,
                  std::vector<karing::dao::KaringRecord>& out) const;

  int count_active() const;
  bool count_search_fts(const std::string& fts_query, long long& out) const;

  std::vector<karing::dao::KaringRecord> list_filtered(int limit, const karing::dao::KaringDao::Filters& filters) const;
  long long count_filtered(const karing::dao::KaringDao::Filters& filters) const;

 private:
  std::string db_path_;
};

}  // namespace karing::repository

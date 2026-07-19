#include "karing_dao_internal.h"

#include "repository/entry_repository.h"

namespace karing::dao {

std::optional<int> KaringDao::latest_id() {
  repository::entry_repository repo(db_path_);
  return repo.latest_id();
}

std::optional<KaringRecord> KaringDao::get_by_id(int id) {
  repository::entry_repository repo(db_path_);
  return repo.get_by_id(id);
}

std::vector<KaringRecord> KaringDao::list_latest(int limit, SortField sort, bool desc) {
  repository::entry_repository repo(db_path_);
  return repo.list_latest(limit, sort, desc);
}

bool KaringDao::try_search_fts(const std::string& fts_query, int limit, SortField sort, bool desc, std::vector<KaringRecord>& out) {
  repository::entry_repository repo(db_path_);
  return repo.search_fts(fts_query, limit, sort, desc, out);
}

int KaringDao::count_active() {
  repository::entry_repository repo(db_path_);
  return repo.count_active();
}

bool KaringDao::count_search_fts(const std::string& fts_query, long long& out) {
  repository::entry_repository repo(db_path_);
  return repo.count_search_fts(fts_query, out);
}

std::vector<KaringRecord> KaringDao::list_filtered(int limit, const Filters& f) {
  repository::entry_repository repo(db_path_);
  return repo.list_filtered(limit, f);
}

long long KaringDao::count_filtered(const Filters& f) {
  repository::entry_repository repo(db_path_);
  return repo.count_filtered(f);
}

}  // namespace karing::dao

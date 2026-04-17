#include "karing_dao_internal.h"

#include "repository/entry_repository.h"
#include "storage/file_storage.h"

namespace karing::dao {

std::optional<int> KaringDao::latest_id() {
  repository::entry_repository repo(db_path_);
  return repo.latest_id();
}

std::optional<KaringRecord> KaringDao::get_by_id(int id) {
  repository::entry_repository repo(db_path_);
  return repo.get_by_id(id);
}

bool KaringDao::get_file_blob(int id, std::string& out_mime, std::string& out_filename, std::string& out_data) {
  repository::entry_repository repo(db_path_);
  KaringRecord record{};
  std::string file_path;
  if (!repo.get_file_record(id, record, file_path) || file_path.empty()) return false;

  if (!karing::storage::file_storage::read(file_path, out_data)) return false;
  out_mime = record.mime.empty() ? "application/octet-stream" : record.mime;
  out_filename = record.filename.empty() ? "download" : record.filename;
  return true;
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

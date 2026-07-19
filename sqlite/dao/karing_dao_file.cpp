#include "karing_dao_internal.h"

#include "repository/entry_repository.h"
#include "storage/file_storage.h"
#include "store/entry_store.h"

namespace karing::dao {

int KaringDao::insert_file(const std::string& filename, const std::string& mime, const std::string& data) {
  store::entry_store store(db_path_, upload_path_);
  return store.insert_file(filename, mime, data);
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

bool KaringDao::update_file(int id, const std::string& filename, const std::string& mime, const std::string& data) {
  store::entry_store store(db_path_, upload_path_);
  return store.update_file(id, filename, mime, data);
}

bool KaringDao::patch_file(int id,
                           const std::optional<std::string>& filename,
                           const std::optional<std::string>& mime,
                           const std::optional<std::string>& data) {
  store::entry_store store(db_path_, upload_path_);
  return store.patch_file(id, filename, mime, data);
}

}  // namespace karing::dao

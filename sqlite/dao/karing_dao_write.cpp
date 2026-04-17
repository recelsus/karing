#include "karing_dao_internal.h"

#include "store/entry_store.h"

namespace karing::dao {

int KaringDao::insert_text(const std::string& content) {
  store::entry_store store(db_path_, upload_path_);
  return store.insert_text(content);
}

bool KaringDao::logical_delete(int id) {
  store::entry_store store(db_path_, upload_path_);
  return store.logical_delete(id);
}

bool KaringDao::logical_delete_latest_recent(int max_age_seconds) {
  store::entry_store store(db_path_, upload_path_);
  return store.logical_delete_latest_recent(max_age_seconds);
}

int KaringDao::insert_file(const std::string& filename, const std::string& mime, const std::string& data) {
  store::entry_store store(db_path_, upload_path_);
  return store.insert_file(filename, mime, data);
}

bool KaringDao::update_text(int id, const std::string& content) {
  store::entry_store store(db_path_, upload_path_);
  return store.update_text(id, content);
}

bool KaringDao::update_file(int id, const std::string& filename, const std::string& mime, const std::string& data) {
  store::entry_store store(db_path_, upload_path_);
  return store.update_file(id, filename, mime, data);
}

bool KaringDao::patch_text(int id, const std::optional<std::string>& content) {
  store::entry_store store(db_path_, upload_path_);
  return store.patch_text(id, content);
}

bool KaringDao::patch_file(int id, const std::optional<std::string>& filename, const std::optional<std::string>& mime, const std::optional<std::string>& data) {
  store::entry_store store(db_path_, upload_path_);
  return store.patch_file(id, filename, mime, data);
}

bool KaringDao::swap_entries(int id1, int id2) {
  store::entry_store store(db_path_, upload_path_);
  return store.swap_entries(id1, id2);
}

}  // namespace karing::dao

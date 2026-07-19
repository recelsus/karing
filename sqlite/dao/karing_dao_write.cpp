#include "karing_dao_internal.h"

#include "store/entry_store.h"

namespace karing::dao {

int KaringDao::insert_text(const std::string& content) {
  store::entry_store store(db_path_);
  return store.insert_text(content);
}

bool KaringDao::logical_delete(int id) {
  store::entry_store store(db_path_);
  return store.logical_delete(id);
}

bool KaringDao::logical_delete_latest_recent(int max_age_seconds) {
  store::entry_store store(db_path_);
  return store.logical_delete_latest_recent(max_age_seconds);
}

bool KaringDao::update_text(int id, const std::string& content) {
  store::entry_store store(db_path_);
  return store.update_text(id, content);
}

bool KaringDao::patch_text(int id, const std::optional<std::string>& content) {
  store::entry_store store(db_path_);
  return store.patch_text(id, content);
}

bool KaringDao::swap_entries(int id1, int id2) {
  store::entry_store store(db_path_);
  return store.swap_entries(id1, id2);
}

std::optional<std::pair<std::vector<KaringRecord>, int>> KaringDao::move_entry_before(int id, int before_id) {
  store::entry_store store(db_path_);
  return store.move_entry_before(id, before_id);
}

std::optional<std::pair<std::vector<KaringRecord>, int>> KaringDao::resequence_entries() {
  store::entry_store store(db_path_);
  return store.resequence_entries();
}

}  // namespace karing::dao

#include "repository/store_state_repository.h"

#include "dao/karing_dao_internal.h"

namespace karing::repository {

store_state_repository::store_state_repository(std::string db_path) : db_path_(std::move(db_path)) {}

bool store_state_repository::fetch_state(int& next_id, int& max_items) const {
  dao::detail::Db db(db_path_);
  if (!db.ok()) return false;
  return dao::detail::fetch_slot_state(db, next_id, max_items);
}

std::optional<int> store_state_repository::previous_slot_id() const {
  dao::detail::Db db(db_path_);
  if (!db.ok()) return std::nullopt;
  return dao::detail::previous_slot_id(db);
}

}  // namespace karing::repository

#pragma once

#include <optional>
#include <string>

namespace karing::repository {

class store_state_repository {
 public:
  explicit store_state_repository(std::string db_path);

  bool fetch_state(int& next_id, int& max_items) const;
  std::optional<int> previous_slot_id() const;

 private:
  std::string db_path_;
};

}  // namespace karing::repository

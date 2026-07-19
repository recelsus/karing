#pragma once

#include <optional>
#include <string>
#include <vector>

#include "dao/karing_dao.h"
#include "domain/entry_operations.h"

namespace karing::services {

using search_error = karing::domain::search_error;
using search_request = karing::domain::search_request;
using search_result = karing::domain::search_result;

class search_service {
 public:
  search_service(std::string db_path, std::string upload_path, int max_limit);

  search_result search(const search_request& request) const;
  search_result live_search(const search_request& request) const;

 private:
  karing::domain::entry_operations make_operations() const;

  std::string db_path_;
  int max_limit_{0};
};

}  // namespace karing::services

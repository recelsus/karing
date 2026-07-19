#include "services/search_service.h"

namespace karing::services {

search_service::search_service(std::string db_path, std::string upload_path, int max_limit)
    : db_path_(std::move(db_path)), upload_path_(std::move(upload_path)), max_limit_(max_limit) {}

karing::domain::entry_operations search_service::make_operations() const {
  return karing::domain::entry_operations(db_path_, upload_path_, max_limit_);
}

search_result search_service::search(const search_request& request) const {
  return make_operations().search(request);
}

search_result search_service::live_search(const search_request& request) const {
  return make_operations().live_search(request);
}

}  // namespace karing::services

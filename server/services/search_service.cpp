#include "services/search_service.h"

#include <algorithm>

#include "utils/search_query.h"

namespace karing::services {

namespace {

std::optional<dao::SortField> parse_sort_field(const std::string& value) {
  if (value.empty() || value == "id") return dao::SortField::id;
  if (value == "stored_at") return dao::SortField::stored_at;
  if (value == "updated_at") return dao::SortField::updated_at;
  return std::nullopt;
}

std::optional<bool> parse_sort_order(const std::string& value) {
  if (value.empty() || value == "desc") return true;
  if (value == "asc") return false;
  return std::nullopt;
}

std::optional<int> parse_type_filter(const std::string& value) {
  if (value == "text") return 0;
  if (value == "file") return 1;
  return std::nullopt;
}

search_result make_error(search_error error, std::optional<std::string> detail_reason = std::nullopt) {
  search_result result;
  result.error = error;
  result.detail_reason = std::move(detail_reason);
  return result;
}

}  // namespace

search_service::search_service(std::string db_path, std::string upload_path, int max_limit)
    : db_path_(std::move(db_path)), upload_path_(std::move(upload_path)), max_limit_(max_limit) {}

karing::dao::KaringDao search_service::make_dao() const {
  return karing::dao::KaringDao(db_path_, upload_path_);
}

search_result search_service::search(const search_request& request) const {
  search_result result;
  result.limit = std::min(std::max(1, request.limit > 0 ? request.limit : max_limit_), max_limit_);
  result.sort = request.sort.empty() ? "id" : request.sort;
  result.order = request.order.empty() ? "desc" : request.order;

  const auto sort = parse_sort_field(request.sort);
  if (!sort.has_value()) return make_error(search_error::invalid_sort);
  const auto order_desc = parse_sort_order(request.order);
  if (!order_desc.has_value()) return make_error(search_error::invalid_order);

  std::optional<int> is_file;
  if (!request.type.empty()) {
    is_file = parse_type_filter(request.type);
    if (!is_file.has_value()) is_file.reset();
  }

  auto dao = make_dao();
  if (!request.q.empty()) {
    auto qb = karing::search::build_fts_query(request.q);
    if (qb.err) return make_error(search_error::invalid_query, *qb.err);

    if (is_file.has_value()) {
      std::vector<dao::KaringRecord> tmp;
      if (!dao.try_search_fts(qb.fts, result.limit, *sort, *order_desc, tmp)) {
        return make_error(search_error::fts_unavailable);
      }
      for (auto& record : tmp) {
        if (static_cast<int>(record.is_file) == *is_file) result.records.push_back(std::move(record));
      }
    } else if (!dao.try_search_fts(qb.fts, result.limit, *sort, *order_desc, result.records)) {
      return make_error(search_error::fts_unavailable);
    }
    return result;
  }

  if (is_file.has_value()) {
    dao::KaringDao::Filters filters;
    filters.is_file = *is_file;
    filters.sort = *sort;
    filters.order_desc = *order_desc;
    result.records = dao.list_filtered(result.limit, filters);
    result.total = dao.count_filtered(filters);
  } else {
    result.records = dao.list_latest(result.limit, *sort, *order_desc);
    result.total = dao.count_active();
  }
  result.has_total = true;
  return result;
}

search_result search_service::live_search(const search_request& request) const {
  if (request.q.empty()) return make_error(search_error::missing_query);

  search_result result;
  result.live = true;
  result.limit = std::min(std::max(1, request.limit > 0 ? request.limit : std::min(max_limit_, 10)), max_limit_);
  result.sort = request.sort.empty() ? "id" : request.sort;
  result.order = request.order.empty() ? "desc" : request.order;

  const auto sort = parse_sort_field(request.sort);
  if (!sort.has_value()) return make_error(search_error::invalid_sort);
  const auto order_desc = parse_sort_order(request.order);
  if (!order_desc.has_value()) return make_error(search_error::invalid_order);

  std::optional<int> is_file;
  if (!request.type.empty()) {
    is_file = parse_type_filter(request.type);
    if (!is_file.has_value()) is_file.reset();
  }

  const auto qb = karing::search::build_live_fts_query(request.q);
  if (qb.err) return make_error(search_error::invalid_query, *qb.err);

  auto dao = make_dao();
  if (is_file.has_value()) {
    std::vector<dao::KaringRecord> tmp;
    if (!dao.try_search_fts(qb.fts, result.limit, *sort, *order_desc, tmp)) {
      return make_error(search_error::fts_unavailable);
    }
    for (auto& record : tmp) {
      if (static_cast<int>(record.is_file) == *is_file) result.records.push_back(std::move(record));
    }
  } else if (!dao.try_search_fts(qb.fts, result.limit, *sort, *order_desc, result.records)) {
    return make_error(search_error::fts_unavailable);
  }

  return result;
}

}  // namespace karing::services

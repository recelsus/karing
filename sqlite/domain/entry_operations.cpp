#include "domain/entry_operations.h"

#include <algorithm>

#include "domain/search_query.h"

namespace karing::domain {

namespace {

std::optional<dao::SortField> to_sort_field(const std::string& value) {
  if (value.empty() || value == "id") return dao::SortField::id;
  if (value == "stored_at") return dao::SortField::stored_at;
  if (value == "updated_at") return dao::SortField::updated_at;
  return std::nullopt;
}

std::optional<bool> to_sort_order_desc(const std::string& value) {
  if (value.empty() || value == "desc") return true;
  if (value == "asc") return false;
  return std::nullopt;
}

std::optional<int> to_type_filter(const std::string& value) {
  if (value == "text") return 0;
  if (value == "file") return 1;
  return std::nullopt;
}

search_result make_search_error(search_error error, std::optional<std::string> detail_reason = std::nullopt) {
  search_result result;
  result.error = error;
  result.detail_reason = std::move(detail_reason);
  return result;
}

}  // namespace

entry_operations::entry_operations(std::string db_path, int max_limit)
    : entry_operations(std::move(db_path), "", max_limit) {}

entry_operations::entry_operations(std::string db_path, std::string upload_path, int max_limit)
    : db_path_(std::move(db_path)), upload_path_(std::move(upload_path)), max_limit_(max_limit) {}

karing::dao::KaringDao entry_operations::make_dao() const {
  return karing::dao::KaringDao(db_path_, upload_path_);
}

std::optional<EntryRecord> entry_operations::latest_record() const {
  auto dao = make_dao();
  const auto latest = dao.latest_id();
  if (!latest) return std::nullopt;
  return dao.get_by_id(*latest);
}

std::optional<EntryRecord> entry_operations::record_by_id(int id) const {
  auto dao = make_dao();
  return dao.get_by_id(id);
}

int entry_operations::create_text(const std::string& content) const {
  auto dao = make_dao();
  return dao.insert_text(content);
}

bool entry_operations::replace_text(int id, const std::string& content) const {
  auto dao = make_dao();
  return dao.update_text(id, content);
}

bool entry_operations::patch_text(int id, const std::optional<std::string>& content) const {
  auto dao = make_dao();
  return dao.patch_text(id, content);
}

bool entry_operations::delete_latest_recent(int max_age_seconds) const {
  auto dao = make_dao();
  return dao.logical_delete_latest_recent(max_age_seconds);
}

bool entry_operations::delete_by_id(int id) const {
  auto dao = make_dao();
  return dao.logical_delete(id);
}

std::optional<std::pair<EntryRecord, EntryRecord>> entry_operations::swap(int id1, int id2) const {
  auto dao = make_dao();
  if (!dao.swap_entries(id1, id2)) return std::nullopt;
  const auto first = dao.get_by_id(id1);
  const auto second = dao.get_by_id(id2);
  if (!first || !second) return std::nullopt;
  return std::make_pair(*first, *second);
}

std::optional<std::pair<std::vector<EntryRecord>, int>> entry_operations::move_before(int id, int before_id) const {
  auto dao = make_dao();
  return dao.move_entry_before(id, before_id);
}

std::optional<std::pair<std::vector<EntryRecord>, int>> entry_operations::resequence() const {
  auto dao = make_dao();
  return dao.resequence_entries();
}

search_result entry_operations::search(const search_request& request) const {
  search_result result;
  result.limit = std::min(std::max(1, request.limit > 0 ? request.limit : max_limit_), max_limit_);
  result.sort = request.sort.empty() ? "id" : request.sort;
  result.order = request.order.empty() ? "desc" : request.order;

  const auto sort = to_sort_field(request.sort);
  if (!sort.has_value()) return make_search_error(search_error::invalid_sort);
  const auto order_desc = to_sort_order_desc(request.order);
  if (!order_desc.has_value()) return make_search_error(search_error::invalid_order);

  std::optional<int> is_file;
  if (!request.type.empty()) {
    is_file = to_type_filter(request.type);
    if (!is_file.has_value()) is_file.reset();
  }

  auto dao = make_dao();
  if (!request.q.empty()) {
    auto query = karing::search::build_fts_query(request.q);
    if (query.err) return make_search_error(search_error::invalid_query, *query.err);

    if (is_file.has_value()) {
      std::vector<EntryRecord> tmp;
      if (!dao.try_search_fts(query.fts, result.limit, *sort, *order_desc, tmp)) {
        return make_search_error(search_error::fts_unavailable);
      }
      for (auto& record : tmp) {
        if (static_cast<int>(record.is_file) == *is_file) result.records.push_back(std::move(record));
      }
    } else if (!dao.try_search_fts(query.fts, result.limit, *sort, *order_desc, result.records)) {
      return make_search_error(search_error::fts_unavailable);
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

search_result entry_operations::live_search(const search_request& request) const {
  if (request.q.empty()) return make_search_error(search_error::missing_query);

  search_result result;
  result.live = true;
  result.limit = std::min(std::max(1, request.limit > 0 ? request.limit : std::min(max_limit_, 10)), max_limit_);
  result.sort = request.sort.empty() ? "id" : request.sort;
  result.order = request.order.empty() ? "desc" : request.order;

  const auto sort = to_sort_field(request.sort);
  if (!sort.has_value()) return make_search_error(search_error::invalid_sort);
  const auto order_desc = to_sort_order_desc(request.order);
  if (!order_desc.has_value()) return make_search_error(search_error::invalid_order);

  std::optional<int> is_file;
  if (!request.type.empty()) {
    is_file = to_type_filter(request.type);
    if (!is_file.has_value()) is_file.reset();
  }

  const auto query = karing::search::build_live_fts_query(request.q);
  if (query.err) return make_search_error(search_error::invalid_query, *query.err);

  auto dao = make_dao();
  if (is_file.has_value()) {
    std::vector<EntryRecord> tmp;
    if (!dao.try_search_fts(query.fts, result.limit, *sort, *order_desc, tmp)) {
      return make_search_error(search_error::fts_unavailable);
    }
    for (auto& record : tmp) {
      if (static_cast<int>(record.is_file) == *is_file) result.records.push_back(std::move(record));
    }
  } else if (!dao.try_search_fts(query.fts, result.limit, *sort, *order_desc, result.records)) {
    return make_search_error(search_error::fts_unavailable);
  }

  return result;
}

}  // namespace karing::domain

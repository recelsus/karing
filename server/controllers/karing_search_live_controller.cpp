#include "karing_search_live_controller.h"

#include <optional>
#include <string>

#include <drogon/drogon.h>

#include "dao/karing_dao.h"
#include "http/record_json.h"
#include "utils/json_response.h"
#include "utils/options.h"
#include "utils/search_query.h"

using drogon::HttpRequestPtr;
using drogon::HttpResponsePtr;
using drogon::HttpStatusCode;

namespace karing::controllers {

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

}  // namespace

void karing_search_live_controller::search_live(const HttpRequestPtr& req, std::function<void(const HttpResponsePtr&)>&& cb) {
  const auto& options = karing::options::current();
  dao::KaringDao dao(options.db_path, options.upload_path);
  auto params = req->getParameters();
  auto get_str = [&](const char* key) -> std::string {
    auto it = params.find(key);
    return it != params.end() ? it->second : std::string();
  };
  auto get_int = [&](const char* key, int fallback) -> int {
    auto it = params.find(key);
    if (it != params.end()) {
      try {
        return std::stoi(it->second);
      } catch (...) {
      }
    }
    return fallback;
  };

  const std::string q = get_str("q");
  if (q.empty()) return cb(karing::http::error(HttpStatusCode::k400BadRequest, "E_QUERY", "q is required"));

  int limit = get_int("limit", std::min(options.limit, 10));
  limit = std::min(std::max(1, limit), options.limit);
  const auto sort = parse_sort_field(get_str("sort"));
  if (!sort.has_value()) return cb(karing::http::error(HttpStatusCode::k400BadRequest, "E_QUERY", "Invalid sort"));
  const auto order_desc = parse_sort_order(get_str("order"));
  if (!order_desc.has_value()) return cb(karing::http::error(HttpStatusCode::k400BadRequest, "E_QUERY", "Invalid order"));

  std::optional<int> is_file;
  std::string type = get_str("type");
  if (type == "text") is_file = 0;
  else if (type == "file") is_file = 1;

  const auto qb = karing::search::build_live_fts_query(q);
  if (qb.err) {
    Json::Value detail;
    detail["reason"] = *qb.err;
    return cb(karing::http::error(HttpStatusCode::k400BadRequest, "E_QUERY", "Invalid live search query", detail));
  }

  std::vector<dao::KaringRecord> list;
  bool ok = false;
  if (is_file.has_value()) {
    std::vector<dao::KaringRecord> tmp;
    ok = dao.try_search_fts(qb.fts, limit, *sort, *order_desc, tmp);
    for (auto& r : tmp) {
      if (static_cast<int>(r.is_file) == *is_file) list.push_back(std::move(r));
    }
  } else {
    ok = dao.try_search_fts(qb.fts, limit, *sort, *order_desc, list);
  }
  if (!ok) return cb(karing::http::error(HttpStatusCode::k503ServiceUnavailable, "E_FTS_UNAVAILABLE", "Full-text search unavailable"));

  Json::Value meta(Json::objectValue);
  meta["count"] = static_cast<int>(list.size());
  meta["limit"] = limit;
  meta["sort"] = get_str("sort").empty() ? "id" : get_str("sort");
  meta["order"] = get_str("order").empty() ? "desc" : get_str("order");
  meta["live"] = true;

  Json::Value data = Json::arrayValue;
  for (auto& record : list) data.append(karing::http::record_to_live_json(record));
  return cb(karing::http::ok(data, meta));
}

}  // namespace karing::controllers

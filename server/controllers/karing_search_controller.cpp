#include "karing_search_controller.h"

#include <string>

#include <drogon/drogon.h>

#include "http/record_json.h"
#include "services/search_service.h"
#include "utils/json_response.h"
#include "utils/options.h"

using drogon::HttpRequestPtr;
using drogon::HttpResponsePtr;
using drogon::HttpStatusCode;

namespace karing::controllers {

void karing_search_controller::search(const HttpRequestPtr& req, std::function<void(const HttpResponsePtr&)>&& cb) {
  const auto& options = karing::options::current();
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

  services::search_service service(options.db_path, options.upload_path, options.limit);
  const auto result = service.search({
      .q = get_str("q"),
      .limit = get_int("limit", options.limit),
      .type = get_str("type"),
      .sort = get_str("sort"),
      .order = get_str("order"),
  });

  switch (result.error) {
    case services::search_error::invalid_sort:
      return cb(karing::http::error(HttpStatusCode::k400BadRequest, "E_QUERY", "Invalid sort"));
    case services::search_error::invalid_order:
      return cb(karing::http::error(HttpStatusCode::k400BadRequest, "E_QUERY", "Invalid order"));
    case services::search_error::invalid_query: {
      Json::Value detail;
      if (result.detail_reason.has_value()) detail["reason"] = *result.detail_reason;
      return cb(karing::http::error(HttpStatusCode::k400BadRequest, "E_QUERY", "Invalid search query", detail));
    }
    case services::search_error::fts_unavailable:
      return cb(karing::http::error(HttpStatusCode::k503ServiceUnavailable, "E_FTS_UNAVAILABLE", "Full-text search unavailable"));
    case services::search_error::none:
    case services::search_error::missing_query:
      break;
  }

  Json::Value meta(Json::objectValue);
  meta["count"] = static_cast<int>(result.records.size());
  meta["limit"] = result.limit;
  meta["sort"] = result.sort;
  meta["order"] = result.order;
  if (result.has_total) meta["total"] = Json::Int64(result.total);

  Json::Value data = Json::arrayValue;
  for (const auto& record : result.records) data.append(karing::http::record_to_json(record));
  return cb(karing::http::ok(data, meta));
}

}  // namespace karing::controllers

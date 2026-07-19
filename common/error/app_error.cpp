#include "common/error/app_error.h"

#include <utility>

namespace karing::domain {

app_error make_error(error_category category,
                     error_code code,
                     std::string message,
                     std::optional<std::string> detail) {
  return {
      .category = category,
      .code = code,
      .message = std::move(message),
      .detail = std::move(detail),
  };
}

const char* to_code_string(error_code code) {
  switch (code) {
    case error_code::none: return "E_NONE";
    case error_code::validation: return "E_VALIDATION";
    case error_code::query: return "E_QUERY";
    case error_code::not_found: return "E_NOT_FOUND";
    case error_code::conflict: return "E_CONFLICT";
    case error_code::size: return "E_SIZE";
    case error_code::mime: return "E_MIME";
    case error_code::backend_unsupported: return "E_BACKEND";
    case error_code::sqlite_open_failed: return "E_SQLITE_OPEN_FAILED";
    case error_code::sqlite_configure_failed: return "E_SQLITE_CONFIGURE_FAILED";
    case error_code::sqlite_busy: return "E_SQLITE_BUSY";
    case error_code::sqlite_constraint: return "E_SQLITE_CONSTRAINT";
    case error_code::sqlite_io: return "E_SQLITE_IO";
    case error_code::sqlite_unknown: return "E_SQLITE";
    case error_code::fts_unavailable: return "E_FTS_UNAVAILABLE";
    case error_code::internal: return "E_INTERNAL";
  }
  return "E_INTERNAL";
}

const char* to_category_string(error_category category) {
  switch (category) {
    case error_category::none: return "none";
    case error_category::validation: return "validation";
    case error_category::query: return "query";
    case error_category::not_found: return "not_found";
    case error_category::conflict: return "conflict";
    case error_category::size: return "size";
    case error_category::mime: return "mime";
    case error_category::backend: return "backend";
    case error_category::database: return "database";
    case error_category::unavailable: return "unavailable";
    case error_category::internal: return "internal";
  }
  return "internal";
}

}  // namespace karing::domain

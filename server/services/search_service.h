#pragma once

#include <optional>
#include <string>
#include <vector>

#include "dao/karing_dao.h"

namespace karing::services {

enum class search_error {
  none,
  missing_query,
  invalid_sort,
  invalid_order,
  invalid_query,
  fts_unavailable,
};

struct search_request {
  std::string q;
  int limit{0};
  std::string type;
  std::string sort;
  std::string order;
};

struct search_result {
  search_error error{search_error::none};
  std::optional<std::string> detail_reason;
  std::vector<karing::dao::KaringRecord> records;
  long long total{0};
  bool has_total{false};
  int limit{0};
  std::string sort{"id"};
  std::string order{"desc"};
  bool live{false};
};

class search_service {
 public:
  search_service(std::string db_path, std::string upload_path, int max_limit);

  search_result search(const search_request& request) const;
  search_result live_search(const search_request& request) const;

 private:
  karing::dao::KaringDao make_dao() const;

  std::string db_path_;
  std::string upload_path_;
  int max_limit_{0};
};

}  // namespace karing::services

#pragma once

#include <optional>
#include <string>

namespace karing::search {

struct QueryBuild {
  std::string fts;
  std::optional<std::string> err;
};

QueryBuild build_fts_query(const std::string& raw);
QueryBuild build_live_fts_query(const std::string& raw);

}  // namespace karing::search

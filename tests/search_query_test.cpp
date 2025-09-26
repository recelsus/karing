#include <catch2/catch_test_macros.hpp>
#include "utils/search_query.h"

using karing::search::build_fts_query;

TEST_CASE("search query basic AND and OR", "[search]") {
  auto q1 = build_fts_query("foo bar");
  REQUIRE_FALSE(q1.err.has_value());
  REQUIRE(q1.fts.find(" AND ") != std::string::npos);

  auto q2 = build_fts_query("foo | bar");
  REQUIRE_FALSE(q2.err.has_value());
  REQUIRE(q2.fts.find(" OR ") != std::string::npos);
}

TEST_CASE("search query quotes and escaping", "[search]") {
  auto bad = build_fts_query("\"hello"); // unclosed quote
  REQUIRE(bad.err.has_value());

  auto ok = build_fts_query("\"hello world\"");
  REQUIRE_FALSE(ok.err.has_value());
  REQUIRE(ok.fts == "\"hello world\"");
}

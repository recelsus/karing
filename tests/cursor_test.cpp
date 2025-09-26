#include <catch2/catch_test_macros.hpp>
#include "utils/cursor.h"

TEST_CASE("cursor build/parse roundtrip", "[cursor]") {
  long long ts = 1712345678LL;
  int id = 42;
  auto s = karing::cursor::build(ts, id);
  auto c = karing::cursor::parse(s);
  REQUIRE(c.has_value());
  REQUIRE(c->created_at == ts);
  REQUIRE(c->id == id);
}

TEST_CASE("cursor parse rejects invalid", "[cursor]") {
  REQUIRE_FALSE(karing::cursor::parse("").has_value());
  REQUIRE_FALSE(karing::cursor::parse(":").has_value());
  REQUIRE_FALSE(karing::cursor::parse("abc:def").has_value());
  REQUIRE_FALSE(karing::cursor::parse("-1:10").has_value());
  REQUIRE_FALSE(karing::cursor::parse("100:0").has_value());
}


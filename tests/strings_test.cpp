#include <catch2/catch_test_macros.hpp>
#include "utils/strings.h"

TEST_CASE("utf8_prefix respects codepoint boundaries", "[strings]") {
  using karing::str::utf8_prefix;
  // ASCII
  REQUIRE(utf8_prefix("hello", 5) == "hello");
  REQUIRE(utf8_prefix("hello", 3) == "hel");
  // Multibyte examples: U+3042 (3 bytes), U+3044 (3 bytes)
  std::string s = "あい"; // 6 bytes total
  REQUIRE(utf8_prefix(s, 6) == s);
  REQUIRE(utf8_prefix(s, 5) == "あ"); // should not cut into second codepoint
  REQUIRE(utf8_prefix(s, 3) == "あ");
  REQUIRE(utf8_prefix(s, 2).empty()); // first codepoint doesn't fit fully
}

TEST_CASE("sha256_hex produces known digest", "[strings]") {
  using karing::str::sha256_hex;
  // SHA-256("abc") = ba7816bf8f01cfea414140de5dae2223b00361a396177a9cb410ff61f20015ad
  REQUIRE(sha256_hex("abc") ==
          std::string("ba7816bf8f01cfea414140de5dae2223b00361a396177a9cb410ff61f20015ad"));
}

TEST_CASE("simple_diff shows +/- lines", "[strings]") {
  using karing::str::simple_diff;
  std::string a = "one\ntwo\nthree\n";
  std::string b = "one\nTWO\nthree\nfour\n";
  auto d = simple_diff(a, b);
  // Should include -two and +TWO and +four
  REQUIRE(d.find("-two") != std::string::npos);
  REQUIRE(d.find("+TWO") != std::string::npos);
  REQUIRE(d.find("+four") != std::string::npos);
}

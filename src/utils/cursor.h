#pragma once
#include <string>
#include <optional>

namespace karing::cursor {

struct Cursor {
  long long created_at{0};
  int id{0};
};

// Format: "<created_at>:<id>" (both base-10). Suitable for URLs.
std::string build(long long created_at, int id);

// Parse from the format above. Returns nullopt if invalid.
std::optional<Cursor> parse(const std::string& s);

}


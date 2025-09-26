#pragma once
#include <string>
#include <optional>

namespace karing::search {

struct QueryBuild {
  std::string fts;                 // FTS5 MATCH string
  std::optional<std::string> err;  // error message if invalid
};

// Build a safe FTS5 MATCH string from user input.
// Rules:
// - Quoted phrases: "hello world"
// - OR: token '|' between terms (e.g., foo | bar)
// - Default operator: AND
// - Prefix: trailing '*' on a term (e.g., foo*)
// - Any term is quoted if it contains non-word characters
// - Very long inputs are truncated to a sensible length
QueryBuild build_fts_query(const std::string& raw);

}


#include "search_query.h"
#include <cctype>
#include <vector>

namespace karing::search {

static std::string trim(const std::string& s) {
  size_t a = 0, b = s.size();
  while (a < b && std::isspace(static_cast<unsigned char>(s[a]))) ++a;
  while (b > a && std::isspace(static_cast<unsigned char>(s[b - 1]))) --b;
  return s.substr(a, b - a);
}

static std::string quote_term(const std::string& t) {
  std::string out; out.reserve(t.size() + 2);
  out.push_back('"');
  for (char c : t) {
    if (c == '"') out.push_back('"'); // double quotes for FTS
    out.push_back(c);
  }
  out.push_back('"');
  return out;
}

QueryBuild build_fts_query(const std::string& raw) {
  QueryBuild qb{};
  std::string s = trim(raw);
  if (s.empty()) { qb.err = "empty"; return qb; }
  // restrict excessively long queries
  if (s.size() > 4096) s = s.substr(0, 4096);

  // Very small parser: supports quotes and | as OR, default AND.
  // Tokens are phrases (quoted) or bare words possibly ending with *.
  bool inQuote = false;
  std::string cur;
  std::vector<std::string> parts;
  for (size_t i = 0; i < s.size(); ++i) {
    char c = s[i];
    if (inQuote) {
      if (c == '"') {
        if (i + 1 < s.size() && s[i + 1] == '"') { cur.push_back('"'); ++i; }
        else { inQuote = false; parts.push_back(quote_term(cur)); cur.clear(); }
      } else {
        cur.push_back(c);
      }
    } else {
      if (c == '"') {
        if (!cur.empty()) { parts.push_back(quote_term(cur)); cur.clear(); }
        inQuote = true;
      } else if (c == '|') {
        if (!cur.empty()) { parts.push_back(quote_term(cur)); cur.clear(); }
        parts.push_back("OR");
      } else if (std::isspace(static_cast<unsigned char>(c))) {
        if (!cur.empty()) { parts.push_back(quote_term(cur)); cur.clear(); }
      } else {
        cur.push_back(c);
      }
    }
  }
  if (inQuote) { qb.err = "unclosed quote"; return qb; }
  if (!cur.empty()) parts.push_back(quote_term(cur));

  // Build final query: insert AND between adjacent terms not separated by OR
  std::string out;
  bool needAnd = false;
  for (const auto& p : parts) {
    if (p == "OR") {
      out.append(" OR ");
      needAnd = false;
    } else {
      if (needAnd) out.append(" AND ");
      out.append(p);
      needAnd = true;
    }
  }
  qb.fts = std::move(out);
  return qb;
}

}

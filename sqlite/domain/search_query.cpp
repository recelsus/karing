#include "domain/search_query.h"

#include <cctype>
#include <vector>

namespace karing::search {

namespace {

std::string trim(const std::string& value) {
  size_t first = 0;
  size_t last = value.size();
  while (first < last && std::isspace(static_cast<unsigned char>(value[first]))) ++first;
  while (last > first && std::isspace(static_cast<unsigned char>(value[last - 1]))) --last;
  return value.substr(first, last - first);
}

std::string quote_term(const std::string& term) {
  std::string out;
  out.reserve(term.size() + 2);
  out.push_back('"');
  for (char ch : term) {
    if (ch == '"') out.push_back('"');
    out.push_back(ch);
  }
  out.push_back('"');
  return out;
}

std::vector<std::string> split_terms(const std::string& raw) {
  std::vector<std::string> terms;
  std::string current;
  for (char ch : raw) {
    if (std::isspace(static_cast<unsigned char>(ch))) {
      if (!current.empty()) {
        terms.push_back(current);
        current.clear();
      }
    } else {
      current.push_back(ch);
    }
  }
  if (!current.empty()) terms.push_back(current);
  return terms;
}

}  // namespace

QueryBuild build_fts_query(const std::string& raw) {
  QueryBuild result{};
  std::string input = trim(raw);
  if (input.empty()) {
    result.err = "empty";
    return result;
  }
  if (input.size() > 4096) input = input.substr(0, 4096);

  bool in_quote = false;
  std::string current;
  std::vector<std::string> parts;
  for (size_t i = 0; i < input.size(); ++i) {
    const char ch = input[i];
    if (in_quote) {
      if (ch == '"') {
        if (i + 1 < input.size() && input[i + 1] == '"') {
          current.push_back('"');
          ++i;
        } else {
          in_quote = false;
          parts.push_back(quote_term(current));
          current.clear();
        }
      } else {
        current.push_back(ch);
      }
      continue;
    }

    if (ch == '"') {
      if (!current.empty()) {
        parts.push_back(quote_term(current));
        current.clear();
      }
      in_quote = true;
    } else if (ch == '|') {
      if (!current.empty()) {
        parts.push_back(quote_term(current));
        current.clear();
      }
      parts.push_back("OR");
    } else if (std::isspace(static_cast<unsigned char>(ch))) {
      if (!current.empty()) {
        parts.push_back(quote_term(current));
        current.clear();
      }
    } else {
      current.push_back(ch);
    }
  }

  if (in_quote) {
    result.err = "unclosed quote";
    return result;
  }
  if (!current.empty()) parts.push_back(quote_term(current));

  bool need_and = false;
  for (const auto& part : parts) {
    if (part == "OR") {
      result.fts.append(" OR ");
      need_and = false;
    } else {
      if (need_and) result.fts.append(" AND ");
      result.fts.append(part);
      need_and = true;
    }
  }
  return result;
}

QueryBuild build_live_fts_query(const std::string& raw) {
  QueryBuild result{};
  std::string input = trim(raw);
  if (input.empty()) {
    result.err = "empty";
    return result;
  }
  if (input.size() > 512) input = input.substr(0, 512);

  const auto terms = split_terms(input);
  if (terms.empty()) {
    result.err = "empty";
    return result;
  }

  for (size_t i = 0; i < terms.size(); ++i) {
    if (i > 0) result.fts.append(" AND ");
    result.fts.append(quote_term(terms[i]));
    result.fts.push_back('*');
  }
  return result;
}

}  // namespace karing::search

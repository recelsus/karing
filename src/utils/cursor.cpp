#include "cursor.h"
#include <cstdlib>

namespace karing::cursor {

std::string build(long long created_at, int id) {
  return std::to_string(created_at) + ":" + std::to_string(id);
}

std::optional<Cursor> parse(const std::string& s) {
  auto pos = s.find(':');
  if (pos == std::string::npos) return std::nullopt;
  auto a = s.substr(0, pos);
  auto b = s.substr(pos + 1);
  if (a.empty() || b.empty()) return std::nullopt;
  char* endp = nullptr;
  long long ts = std::strtoll(a.c_str(), &endp, 10);
  if (!endp || *endp != '\0') return std::nullopt;
  endp = nullptr;
  long idll = std::strtol(b.c_str(), &endp, 10);
  if (!endp || *endp != '\0') return std::nullopt;
  if (ts < 0 || idll <= 0) return std::nullopt;
  Cursor c{ts, (int)idll};
  return c;
}

}


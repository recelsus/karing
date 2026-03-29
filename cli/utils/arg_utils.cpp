#include "utils/arg_utils.h"

#include <string>
#include <vector>

namespace karing::cli::utils {

std::string join_words(const std::vector<std::string>& items) {
  std::string out;
  for (size_t i = 0; i < items.size(); ++i) {
    if (i > 0) out += ' ';
    out += items[i];
  }
  return out;
}

bool is_valid_id(const std::string& value) {
  if (value.empty()) return false;
  for (char ch : value) {
    if (ch < '0' || ch > '9') return false;
  }
  try {
    const int id = std::stoi(value);
    return id >= 1 && id <= 1000;
  } catch (...) {
    return false;
  }
}

}

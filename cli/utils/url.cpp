#include "utils/url.h"

namespace karing::cli::utils {

std::string normalize_base_url(const std::string& raw) {
  std::string out = raw;
  while (out.size() > 1 && out.back() == '/') out.pop_back();
  return out;
}

}

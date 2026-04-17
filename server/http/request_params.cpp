#include "http/request_params.h"

namespace karing::http {

int_param_result parse_int_param(const drogon::SafeStringMap<std::string>& params, const char* key) {
  const auto it = params.find(key);
  if (it == params.end()) return {};
  try {
    return {int_param_status::ok, std::stoi(it->second)};
  } catch (...) {
    return {int_param_status::invalid, 0};
  }
}

}

#pragma once

#include <string>

#include <drogon/drogon.h>

namespace karing::http {

enum class int_param_status {
  missing,
  invalid,
  ok,
};

struct int_param_result {
  int_param_status status{int_param_status::missing};
  int value{0};
};

int_param_result parse_int_param(const drogon::SafeStringMap<std::string>& params, const char* key);

}

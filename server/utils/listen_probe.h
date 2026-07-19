#pragma once

#include <string>

namespace karing::listen_probe {

struct result {
  bool ok{false};
  std::string error;
};

result check_available(const std::string& address, int port);

}  // namespace karing::listen_probe

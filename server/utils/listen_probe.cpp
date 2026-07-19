#include "utils/listen_probe.h"

#include <cstring>
#include <memory>
#include <string>

#if !defined(_WIN32)
#include <cerrno>
#include <netdb.h>
#include <sys/socket.h>
#include <unistd.h>
#endif

namespace karing::listen_probe {

namespace {

#if !defined(_WIN32)

struct addrinfo_deleter {
  void operator()(addrinfo* value) const {
    if (value) freeaddrinfo(value);
  }
};

std::string format_error(const std::string& address, int port, const std::string& message) {
  return "failed to bind " + address + ":" + std::to_string(port) + ": " + message;
}

#endif

}  // namespace

result check_available(const std::string& address, int port) {
  if (address.empty()) return {.ok = false, .error = "listen address is empty"};
  if (port < 1 || port > 65535) return {.ok = false, .error = "listen port must be in range 1..65535"};

#if defined(_WIN32)
  return {.ok = true};
#else
  addrinfo hints{};
  hints.ai_family = AF_UNSPEC;
  hints.ai_socktype = SOCK_STREAM;
  hints.ai_flags = AI_PASSIVE;

  addrinfo* raw_info = nullptr;
  const std::string port_text = std::to_string(port);
  const int gai = getaddrinfo(address.c_str(), port_text.c_str(), &hints, &raw_info);
  if (gai != 0) return {.ok = false, .error = format_error(address, port, gai_strerror(gai))};

  std::unique_ptr<addrinfo, addrinfo_deleter> info(raw_info);
  std::string last_error = "no address candidates";
  for (addrinfo* current = info.get(); current != nullptr; current = current->ai_next) {
    const int fd = socket(current->ai_family, current->ai_socktype, current->ai_protocol);
    if (fd < 0) {
      last_error = std::strerror(errno);
      continue;
    }

    const int ok = bind(fd, current->ai_addr, current->ai_addrlen);
    const int saved_errno = errno;
    close(fd);
    if (ok == 0) return {.ok = true};
    last_error = std::strerror(saved_errno);
  }

  return {.ok = false, .error = format_error(address, port, last_error)};
#endif
}

}  // namespace karing::listen_probe

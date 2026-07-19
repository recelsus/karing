#pragma once

#include <optional>
#include <string>

namespace karing::cli::utils {

void print_help(const std::optional<std::string>& target = std::nullopt);
void print_version();

}

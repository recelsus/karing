#pragma once

#include <optional>
#include <string>

namespace karing::cli::utils {

std::optional<std::string> guess_mime_type(const std::string& path);

}

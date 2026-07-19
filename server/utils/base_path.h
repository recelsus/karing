#pragma once

#include <string>

namespace karing::base_path {

std::string normalize(const std::string& raw);
bool matches(const std::string& normalized_base, const std::string& path);
std::string strip(const std::string& normalized_base, const std::string& path);

}  // namespace karing::base_path

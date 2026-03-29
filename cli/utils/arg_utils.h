#pragma once

#include <string>
#include <vector>

namespace karing::cli::utils {

std::string join_words(const std::vector<std::string>& items);
bool is_valid_id(const std::string& value);

}

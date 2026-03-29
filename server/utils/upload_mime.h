#pragma once

#include <string>
#include <string_view>

namespace karing::upload_mime {

bool is_supported(std::string_view mime);
std::string normalise(std::string_view mime, std::string_view filename);

}

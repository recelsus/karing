#pragma once
#include <string>

namespace karing::str {

// Returns UTF-8 safe prefix, at most max_bytes, not splitting code points.
std::string utf8_prefix(const std::string& s, size_t max_bytes);

// Returns lowercase hex SHA-256 of input bytes.
std::string sha256_hex(const void* data, size_t len);
inline std::string sha256_hex(const std::string& s) { return sha256_hex(s.data(), s.size()); }

// Very simple unified-like diff (line-based): returns a short string of +/- lines.
// If too large or inputs huge, returns empty string.
std::string simple_diff(const std::string& before, const std::string& after, size_t max_output = 4096);

}

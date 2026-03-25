#pragma once

namespace karing::limits {

inline constexpr int kDefaultLimit = 100;
inline constexpr int kMaxLimit = 1000;

inline constexpr int kBytesPerMb = 1024 * 1024;

inline constexpr int kDefaultMaxFileMb = 10;
inline constexpr int kMaxFileMb = 100;
inline constexpr int kDefaultMaxFileBytes = kDefaultMaxFileMb * kBytesPerMb;
inline constexpr int kMaxFileBytes = kMaxFileMb * kBytesPerMb;

inline constexpr int kDefaultMaxTextMb = 1;
inline constexpr int kMaxTextMb = 10;
inline constexpr int kDefaultMaxTextBytes = kDefaultMaxTextMb * kBytesPerMb;
inline constexpr int kMaxTextBytes = kMaxTextMb * kBytesPerMb;

}  // namespace karing::limits

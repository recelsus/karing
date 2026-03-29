#include "utils/upload_mime.h"

#include <algorithm>
#include <map>
#include <string>

namespace karing::upload_mime {

namespace {

bool starts_with(std::string_view value, std::string_view prefix) {
  return value.rfind(prefix, 0) == 0;
}

std::string lower_copy(std::string_view value) {
  std::string out(value);
  std::transform(out.begin(), out.end(), out.begin(), [](unsigned char ch) {
    return static_cast<char>(std::tolower(ch));
  });
  return out;
}

std::string file_extension(std::string_view filename) {
  const auto lower = lower_copy(filename);
  const auto pos = lower.find_last_of('.');
  if (pos == std::string::npos) return {};
  return lower.substr(pos);
}

const std::map<std::string, std::string>& text_extension_map() {
  static const std::map<std::string, std::string> kMap = {
      {".txt", "text/plain"},
      {".md", "text/markdown"},
      {".rst", "text/plain"},
      {".log", "text/plain"},
      {".csv", "text/csv"},
      {".json", "application/json"},
      {".xml", "application/xml"},
      {".yaml", "application/yaml"},
      {".yml", "application/yaml"},
      {".toml", "application/toml"},
      {".ini", "text/plain"},
      {".cfg", "text/plain"},
      {".conf", "text/plain"},
      {".lua", "text/plain"},
      {".py", "text/plain"},
      {".rb", "text/plain"},
      {".pl", "text/plain"},
      {".sh", "text/plain"},
      {".bash", "text/plain"},
      {".zsh", "text/plain"},
      {".fish", "text/plain"},
      {".js", "application/javascript"},
      {".mjs", "application/javascript"},
      {".cjs", "application/javascript"},
      {".ts", "text/plain"},
      {".tsx", "text/plain"},
      {".jsx", "text/plain"},
      {".c", "text/plain"},
      {".cc", "text/plain"},
      {".cpp", "text/plain"},
      {".cxx", "text/plain"},
      {".h", "text/plain"},
      {".hh", "text/plain"},
      {".hpp", "text/plain"},
      {".hxx", "text/plain"},
      {".java", "text/plain"},
      {".kt", "text/plain"},
      {".go", "text/plain"},
      {".rs", "text/plain"},
      {".swift", "text/plain"},
      {".php", "text/plain"},
      {".sql", "text/plain"},
      {".cmake", "text/plain"},
      {".nix", "text/plain"},
      {".vim", "text/plain"},
  };
  return kMap;
}

bool is_text_like_application(std::string_view mime) {
  return mime == "application/json" ||
         mime == "application/ld+json" ||
         mime == "application/xml" ||
         mime == "application/yaml" ||
         mime == "application/x-yaml" ||
         mime == "application/toml" ||
         mime == "application/javascript";
}

}  // namespace

std::string normalise(std::string_view mime, std::string_view filename) {
  std::string out = lower_copy(mime);
  if (!out.empty() && out != "application/octet-stream") return out;

  const auto ext = file_extension(filename);
  const auto it = text_extension_map().find(ext);
  if (it != text_extension_map().end()) return it->second;
  return out;
}

bool is_supported(std::string_view mime) {
  return starts_with(mime, "text/") ||
         starts_with(mime, "image/") ||
         starts_with(mime, "audio/") ||
         starts_with(mime, "video/") ||
         is_text_like_application(mime) ||
         mime == "application/pdf" ||
         mime == "application/zip" ||
         mime == "application/gzip" ||
         mime == "application/x-tar" ||
         mime == "application/x-7z-compressed" ||
         mime == "application/vnd.rar" ||
         mime == "application/msword" ||
         mime == "application/vnd.openxmlformats-officedocument.wordprocessingml.document" ||
         mime == "application/vnd.ms-excel" ||
         mime == "application/vnd.openxmlformats-officedocument.spreadsheetml.sheet" ||
         mime == "application/vnd.ms-powerpoint" ||
         mime == "application/vnd.openxmlformats-officedocument.presentationml.presentation";
}

}

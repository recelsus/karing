#include "utils/mime.h"

#include <algorithm>
#include <filesystem>
#include <fstream>
#include <map>
#include <string>

namespace karing::cli::utils {

namespace {

bool looks_like_text_file(const std::string& path) {
  std::ifstream in(path, std::ios::binary);
  if (!in.is_open()) return false;

  constexpr std::streamsize kProbeSize = 4096;
  std::string buffer(static_cast<size_t>(kProbeSize), '\0');
  in.read(&buffer[0], kProbeSize);
  const auto read_size = in.gcount();
  buffer.resize(static_cast<size_t>(read_size));

  if (buffer.empty()) return true;

  for (unsigned char ch : buffer) {
    if (ch == 0) return false;
    if (ch == '\n' || ch == '\r' || ch == '\t') continue;
    if (ch >= 32 && ch <= 126) continue;
    if (ch >= 128) continue;
    return false;
  }
  return true;
}

}  // namespace

std::optional<std::string> guess_mime_type(const std::string& path) {
  namespace fs = std::filesystem;
  static const std::map<std::string, std::string> kMap = {
      {".txt", "text/plain"},
      {".md", "text/markdown"},
      {".rst", "text/plain"},
      {".log", "text/plain"},
      {".conf", "text/plain"},
      {".cfg", "text/plain"},
      {".ini", "text/plain"},
      {".yaml", "text/plain"},
      {".yml", "text/plain"},
      {".toml", "text/plain"},
      {".xml", "text/plain"},
      {".html", "text/plain"},
      {".htm", "text/plain"},
      {".css", "text/plain"},
      {".js", "text/plain"},
      {".mjs", "text/plain"},
      {".cjs", "text/plain"},
      {".ts", "text/plain"},
      {".tsx", "text/plain"},
      {".jsx", "text/plain"},
      {".lua", "text/plain"},
      {".py", "text/plain"},
      {".rb", "text/plain"},
      {".pl", "text/plain"},
      {".sh", "text/plain"},
      {".bash", "text/plain"},
      {".zsh", "text/plain"},
      {".fish", "text/plain"},
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
      {".json", "text/plain"},
      {".csv", "text/csv"},
      {".png", "image/png"},
      {".jpg", "image/jpeg"},
      {".jpeg", "image/jpeg"},
      {".gif", "image/gif"},
      {".webp", "image/webp"},
      {".svg", "image/svg+xml"},
      {".mp3", "audio/mpeg"},
      {".wav", "audio/wav"},
      {".ogg", "audio/ogg"},
      {".m4a", "audio/mp4"},
      {".mp4", "video/mp4"},
      {".webm", "video/webm"},
      {".pdf", "application/pdf"},
      {".zip", "application/zip"},
      {".gz", "application/gzip"},
      {".tar", "application/x-tar"},
      {".7z", "application/x-7z-compressed"},
      {".rar", "application/vnd.rar"},
      {".doc", "application/msword"},
      {".docx", "application/vnd.openxmlformats-officedocument.wordprocessingml.document"},
      {".xls", "application/vnd.ms-excel"},
      {".xlsx", "application/vnd.openxmlformats-officedocument.spreadsheetml.sheet"},
      {".ppt", "application/vnd.ms-powerpoint"},
      {".pptx", "application/vnd.openxmlformats-officedocument.presentationml.presentation"},
  };
  std::string ext = fs::path(path).extension().string();
  std::transform(ext.begin(), ext.end(), ext.begin(), [](unsigned char ch) { return static_cast<char>(std::tolower(ch)); });
  const auto it = kMap.find(ext);
  if (it != kMap.end()) return it->second;
  if (looks_like_text_file(path)) return std::string("text/plain");
  return std::nullopt;
}

}

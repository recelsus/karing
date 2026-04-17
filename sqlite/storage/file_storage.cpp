#include "storage/file_storage.h"

#include <chrono>
#include <filesystem>
#include <fstream>

namespace fs = std::filesystem;

namespace karing::storage {

file_storage::file_storage(std::string root) : root_(std::move(root)) {}

bool file_storage::write_for_slot(int id, const std::string& data, std::string& out_path) const {
  if (root_.empty()) return false;
  std::error_code ec;
  fs::create_directories(root_, ec);
  if (ec) return false;

  const auto stamp = std::chrono::steady_clock::now().time_since_epoch().count();
  out_path = (fs::path(root_) / ("entry_" + std::to_string(id) + "_" + std::to_string(stamp))).string();
  std::ofstream ofs(out_path, std::ios::binary | std::ios::trunc);
  if (!ofs.is_open()) return false;
  ofs.write(data.data(), static_cast<std::streamsize>(data.size()));
  return ofs.good();
}

bool file_storage::read(const std::string& path, std::string& out_data) {
  std::ifstream ifs(path, std::ios::binary);
  if (!ifs.is_open()) return false;
  out_data.assign(std::istreambuf_iterator<char>(ifs), std::istreambuf_iterator<char>());
  return true;
}

void file_storage::remove_if_any(const std::string& path) {
  if (path.empty()) return;
  std::error_code ec;
  fs::remove(path, ec);
}

}  // namespace karing::storage

#include "storage/file_storage.h"

#include <atomic>
#include <chrono>
#include <filesystem>
#include <fstream>

namespace fs = std::filesystem;

namespace karing::storage {
namespace {

std::string make_storage_name() {
  static std::atomic<unsigned long long> sequence{0};
  const auto stamp = std::chrono::steady_clock::now().time_since_epoch().count();
  return "file_" + std::to_string(stamp) + "_" + std::to_string(sequence.fetch_add(1));
}

}  // namespace

file_storage::file_storage(std::string root) : root_(std::move(root)) {}

bool file_storage::write(const std::string& data, std::string& out_path) const {
  if (root_.empty()) return false;
  std::error_code ec;
  fs::create_directories(root_, ec);
  if (ec) return false;

  out_path = (fs::path(root_) / make_storage_name()).string();
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

bool file_storage::remove_if_any(const std::string& path) {
  if (path.empty()) return true;
  std::error_code ec;
  fs::remove(path, ec);
  return !ec;
}

}  // namespace karing::storage

#pragma once

#include <string>

namespace karing::storage {

class file_storage {
 public:
  explicit file_storage(std::string root);

  bool write_for_slot(int id, const std::string& data, std::string& out_path) const;

  static bool read(const std::string& path, std::string& out_data);
  static void remove_if_any(const std::string& path);

 private:
  std::string root_;
};

}  // namespace karing::storage

#pragma once

#include <optional>
#include <string>

#include "domain/entry_operations.h"

namespace karing::services {

using file_blob = karing::domain::file_blob;

class file_record_service {
 public:
  file_record_service(std::string db_path, std::string upload_path);

  bool file_blob_by_id(int id, file_blob& out) const;
  int create_file(const std::string& filename, const std::string& mime, const std::string& data) const;
  bool replace_file(int id, const std::string& filename, const std::string& mime, const std::string& data) const;
  bool replace_text(int id, const std::string& content) const;
  bool patch_file(int id,
                  const std::optional<std::string>& filename,
                  const std::optional<std::string>& mime,
                  const std::optional<std::string>& data) const;

  bool delete_latest_recent(int max_age_seconds) const;
  bool delete_by_id(int id) const;

 private:
  karing::domain::entry_operations make_operations() const;
  std::string file_path_by_id(int id) const;

  std::string db_path_;
  std::string upload_path_;
};

}  // namespace karing::services

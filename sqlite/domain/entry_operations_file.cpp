#include "domain/entry_operations.h"

#include <utility>

namespace karing::domain {

entry_operations::entry_operations(std::string db_path, std::string upload_path, int max_limit)
    : db_path_(std::move(db_path)), upload_path_(std::move(upload_path)), max_limit_(max_limit) {}

bool entry_operations::file_blob_by_id(int id, file_blob& out) const {
  karing::dao::KaringDao dao(db_path_, upload_path_);
  return dao.get_file_blob(id, out.mime, out.filename, out.data);
}

int entry_operations::create_file(const std::string& filename, const std::string& mime, const std::string& data) const {
  karing::dao::KaringDao dao(db_path_, upload_path_);
  return dao.insert_file(filename, mime, data);
}

bool entry_operations::replace_file(int id, const std::string& filename, const std::string& mime, const std::string& data) const {
  karing::dao::KaringDao dao(db_path_, upload_path_);
  return dao.update_file(id, filename, mime, data);
}

bool entry_operations::patch_file(int id,
                                  const std::optional<std::string>& filename,
                                  const std::optional<std::string>& mime,
                                  const std::optional<std::string>& data) const {
  karing::dao::KaringDao dao(db_path_, upload_path_);
  return dao.patch_file(id, filename, mime, data);
}

}  // namespace karing::domain

#include "domain/entry_operations.h"

namespace karing::domain {

bool entry_operations::file_blob_by_id(int id, file_blob& out) const {
  auto dao = make_dao();
  return dao.get_file_blob(id, out.mime, out.filename, out.data);
}

int entry_operations::create_file(const std::string& filename, const std::string& mime, const std::string& data) const {
  auto dao = make_dao();
  return dao.insert_file(filename, mime, data);
}

bool entry_operations::replace_file(int id, const std::string& filename, const std::string& mime, const std::string& data) const {
  auto dao = make_dao();
  return dao.update_file(id, filename, mime, data);
}

bool entry_operations::patch_file(int id,
                                  const std::optional<std::string>& filename,
                                  const std::optional<std::string>& mime,
                                  const std::optional<std::string>& data) const {
  auto dao = make_dao();
  return dao.patch_file(id, filename, mime, data);
}

}  // namespace karing::domain

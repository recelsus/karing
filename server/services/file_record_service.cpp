#include "services/file_record_service.h"

#include "repository/entry_repository.h"
#include "storage/file_storage.h"

namespace karing::services {

file_record_service::file_record_service(std::string db_path, std::string upload_path)
    : db_path_(std::move(db_path)), upload_path_(std::move(upload_path)) {}

karing::domain::entry_operations file_record_service::make_operations() const {
  return karing::domain::entry_operations(db_path_, upload_path_);
}

std::string file_record_service::file_path_by_id(int id) const {
  karing::repository::entry_repository repo(db_path_);
  karing::dao::KaringRecord record{};
  std::string file_path;
  if (!repo.get_file_record(id, record, file_path)) return {};
  return file_path;
}

bool file_record_service::file_blob_by_id(int id, file_blob& out) const {
  return make_operations().file_blob_by_id(id, out);
}

int file_record_service::create_file(const std::string& filename, const std::string& mime, const std::string& data) const {
  return make_operations().create_file(filename, mime, data);
}

bool file_record_service::replace_file(int id,
                                       const std::string& filename,
                                       const std::string& mime,
                                       const std::string& data) const {
  return make_operations().replace_file(id, filename, mime, data);
}

bool file_record_service::replace_text(int id, const std::string& content) const {
  const std::string file_path = file_path_by_id(id);
  if (!karing::storage::file_storage::remove_if_any(file_path)) return false;
  const bool replaced = make_operations().replace_text(id, content);
  return replaced;
}

bool file_record_service::patch_file(int id,
                                     const std::optional<std::string>& filename,
                                     const std::optional<std::string>& mime,
                                     const std::optional<std::string>& data) const {
  return make_operations().patch_file(id, filename, mime, data);
}

bool file_record_service::delete_latest_recent(int max_age_seconds) const {
  const auto latest = make_operations().latest_record();
  const std::string file_path = latest.has_value() ? file_path_by_id(latest->id) : std::string();
  if (!karing::storage::file_storage::remove_if_any(file_path)) return false;
  const bool deleted = make_operations().delete_latest_recent(max_age_seconds);
  return deleted;
}

bool file_record_service::delete_by_id(int id) const {
  const std::string file_path = file_path_by_id(id);
  if (!karing::storage::file_storage::remove_if_any(file_path)) return false;
  const bool deleted = make_operations().delete_by_id(id);
  return deleted;
}

}  // namespace karing::services

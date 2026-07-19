#include "services/root_service.h"

#include "repository/entry_repository.h"
#include "storage/file_storage.h"

namespace karing::services {
namespace {

std::string file_path_by_id(const std::string& db_path, int id) {
  karing::repository::entry_repository repo(db_path);
  karing::dao::KaringRecord record{};
  std::string file_path;
  if (!repo.get_file_record(id, record, file_path)) return {};
  return file_path;
}

}  // namespace

root_service::root_service(std::string db_path, std::string upload_path)
    : db_path_(std::move(db_path)), upload_path_(std::move(upload_path)) {}

karing::domain::entry_operations root_service::make_operations() const {
  return karing::domain::entry_operations(db_path_, upload_path_);
}

std::optional<karing::dao::KaringRecord> root_service::latest_record() const {
  return make_operations().latest_record();
}

std::optional<karing::dao::KaringRecord> root_service::record_by_id(int id) const {
  return make_operations().record_by_id(id);
}

bool root_service::file_blob_by_id(int id, file_blob& out) const {
  return make_operations().file_blob_by_id(id, out);
}

int root_service::create_text(const std::string& content) const {
  return make_operations().create_text(content);
}

int root_service::create_file(const std::string& filename, const std::string& mime, const std::string& data) const {
  return make_operations().create_file(filename, mime, data);
}

bool root_service::replace_text(int id, const std::string& content) const {
  const std::string file_path = file_path_by_id(db_path_, id);
  const bool replaced = make_operations().replace_text(id, content);
  if (replaced) karing::storage::file_storage::remove_if_any(file_path);
  return replaced;
}

bool root_service::replace_file(int id, const std::string& filename, const std::string& mime, const std::string& data) const {
  return make_operations().replace_file(id, filename, mime, data);
}

bool root_service::patch_text(int id, const std::optional<std::string>& content) const {
  return make_operations().patch_text(id, content);
}

bool root_service::patch_file(int id,
                              const std::optional<std::string>& filename,
                              const std::optional<std::string>& mime,
                              const std::optional<std::string>& data) const {
  return make_operations().patch_file(id, filename, mime, data);
}

bool root_service::delete_latest_recent(int max_age_seconds) const {
  const auto record = latest_record();
  const std::string file_path = record.has_value() ? file_path_by_id(db_path_, record->id) : std::string();
  const bool deleted = make_operations().delete_latest_recent(max_age_seconds);
  if (deleted) karing::storage::file_storage::remove_if_any(file_path);
  return deleted;
}

bool root_service::delete_by_id(int id) const {
  const std::string file_path = file_path_by_id(db_path_, id);
  const bool deleted = make_operations().delete_by_id(id);
  if (deleted) karing::storage::file_storage::remove_if_any(file_path);
  return deleted;
}

std::optional<std::pair<karing::dao::KaringRecord, karing::dao::KaringRecord>> root_service::swap(int id1, int id2) const {
  return make_operations().swap(id1, id2);
}

std::optional<std::pair<std::vector<karing::dao::KaringRecord>, int>> root_service::move_before(int id, int before_id) const {
  return make_operations().move_before(id, before_id);
}

std::optional<std::pair<std::vector<karing::dao::KaringRecord>, int>> root_service::resequence() const {
  return make_operations().resequence();
}

}

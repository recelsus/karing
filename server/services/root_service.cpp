#include "services/root_service.h"

namespace karing::services {

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
  return make_operations().replace_text(id, content);
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
  return make_operations().delete_latest_recent(max_age_seconds);
}

bool root_service::delete_by_id(int id) const {
  return make_operations().delete_by_id(id);
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

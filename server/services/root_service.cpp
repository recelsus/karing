#include "services/root_service.h"

namespace karing::services {

root_service::root_service(std::string db_path, std::string upload_path)
    : db_path_(std::move(db_path)), upload_path_(std::move(upload_path)) {}

karing::dao::KaringDao root_service::make_dao() const {
  return karing::dao::KaringDao(db_path_, upload_path_);
}

std::optional<karing::dao::KaringRecord> root_service::latest_record() const {
  auto dao = make_dao();
  const auto latest = dao.latest_id();
  if (!latest) return std::nullopt;
  return dao.get_by_id(*latest);
}

std::optional<karing::dao::KaringRecord> root_service::record_by_id(int id) const {
  auto dao = make_dao();
  return dao.get_by_id(id);
}

bool root_service::file_blob_by_id(int id, file_blob& out) const {
  auto dao = make_dao();
  return dao.get_file_blob(id, out.mime, out.filename, out.data);
}

int root_service::create_text(const std::string& content) const {
  auto dao = make_dao();
  return dao.insert_text(content);
}

int root_service::create_file(const std::string& filename, const std::string& mime, const std::string& data) const {
  auto dao = make_dao();
  return dao.insert_file(filename, mime, data);
}

bool root_service::replace_text(int id, const std::string& content) const {
  auto dao = make_dao();
  return dao.update_text(id, content);
}

bool root_service::replace_file(int id, const std::string& filename, const std::string& mime, const std::string& data) const {
  auto dao = make_dao();
  return dao.update_file(id, filename, mime, data);
}

bool root_service::patch_text(int id, const std::optional<std::string>& content) const {
  auto dao = make_dao();
  return dao.patch_text(id, content);
}

bool root_service::patch_file(int id,
                              const std::optional<std::string>& filename,
                              const std::optional<std::string>& mime,
                              const std::optional<std::string>& data) const {
  auto dao = make_dao();
  return dao.patch_file(id, filename, mime, data);
}

bool root_service::delete_latest_recent(int max_age_seconds) const {
  auto dao = make_dao();
  return dao.logical_delete_latest_recent(max_age_seconds);
}

bool root_service::delete_by_id(int id) const {
  auto dao = make_dao();
  return dao.logical_delete(id);
}

std::optional<std::pair<karing::dao::KaringRecord, karing::dao::KaringRecord>> root_service::swap(int id1, int id2) const {
  auto dao = make_dao();
  if (!dao.swap_entries(id1, id2)) return std::nullopt;
  const auto first = dao.get_by_id(id1);
  const auto second = dao.get_by_id(id2);
  if (!first || !second) return std::nullopt;
  return std::make_pair(*first, *second);
}

std::optional<std::pair<std::vector<karing::dao::KaringRecord>, int>> root_service::resequence() const {
  auto dao = make_dao();
  return dao.resequence_entries();
}

}

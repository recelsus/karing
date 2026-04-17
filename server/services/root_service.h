#pragma once

#include <optional>
#include <string>
#include <utility>

#include "dao/karing_dao.h"

namespace karing::services {

struct file_blob {
  std::string mime;
  std::string filename;
  std::string data;
};

class root_service {
 public:
  root_service(std::string db_path, std::string upload_path);

  std::optional<karing::dao::KaringRecord> latest_record() const;
  std::optional<karing::dao::KaringRecord> record_by_id(int id) const;
  bool file_blob_by_id(int id, file_blob& out) const;

  int create_text(const std::string& content) const;
  int create_file(const std::string& filename, const std::string& mime, const std::string& data) const;

  bool replace_text(int id, const std::string& content) const;
  bool replace_file(int id, const std::string& filename, const std::string& mime, const std::string& data) const;

  bool patch_text(int id, const std::optional<std::string>& content) const;
  bool patch_file(int id,
                  const std::optional<std::string>& filename,
                  const std::optional<std::string>& mime,
                  const std::optional<std::string>& data) const;

  bool delete_latest_recent(int max_age_seconds) const;
  bool delete_by_id(int id) const;

  std::optional<std::pair<karing::dao::KaringRecord, karing::dao::KaringRecord>> swap(int id1, int id2) const;
  std::optional<std::pair<std::vector<karing::dao::KaringRecord>, int>> resequence() const;

 private:
  karing::dao::KaringDao make_dao() const;

  std::string db_path_;
  std::string upload_path_;
};

}

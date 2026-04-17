#pragma once

#include <optional>
#include <string>
#include <utility>
#include <vector>

#include "dao/karing_dao.h"

namespace karing::store {

class entry_store {
 public:
  entry_store(std::string db_path, std::string upload_path);

  int insert_text(const std::string& content) const;
  int insert_file(const std::string& filename, const std::string& mime, const std::string& data) const;

  bool logical_delete(int id) const;
  bool logical_delete_latest_recent(int max_age_seconds) const;

  bool update_text(int id, const std::string& content) const;
  bool update_file(int id, const std::string& filename, const std::string& mime, const std::string& data) const;

  bool patch_text(int id, const std::optional<std::string>& content) const;
  bool patch_file(int id,
                  const std::optional<std::string>& filename,
                  const std::optional<std::string>& mime,
                  const std::optional<std::string>& data) const;

  bool swap_entries(int id1, int id2) const;
  std::optional<std::pair<std::vector<karing::dao::KaringRecord>, int>> resequence_entries() const;

 private:
  std::string db_path_;
  std::string upload_path_;
};

}  // namespace karing::store

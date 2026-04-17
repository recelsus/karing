#include "http/record_json.h"

#include <algorithm>

namespace karing::http {

Json::Value record_to_json(const karing::dao::KaringRecord& record) {
  Json::Value out;
  out["id"] = record.id;
  out["is_file"] = record.is_file;
  if (!record.is_file && !record.content.empty()) out["content"] = record.content;
  if (!record.filename.empty()) out["filename"] = record.filename;
  if (!record.mime.empty()) out["mime"] = record.mime;
  out["created_at"] = Json::Int64(record.created_at);
  if (record.updated_at) out["updated_at"] = Json::Int64(*record.updated_at);
  return out;
}

Json::Value record_to_live_json(const karing::dao::KaringRecord& record) {
  Json::Value out;
  out["id"] = record.id;
  out["is_file"] = record.is_file;
  if (!record.filename.empty()) out["filename"] = record.filename;
  if (!record.mime.empty()) out["mime"] = record.mime;
  if (!record.is_file && !record.content.empty()) {
    const auto preview_size = std::min<size_t>(record.content.size(), 120);
    out["preview"] = record.content.substr(0, preview_size);
  }
  out["created_at"] = Json::Int64(record.created_at);
  if (record.updated_at) out["updated_at"] = Json::Int64(*record.updated_at);
  return out;
}

}

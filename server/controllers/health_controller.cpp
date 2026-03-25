#include "health_controller.h"

#include <string>

#include <drogon/drogon.h>

#include "db/db_introspection.h"
#include "utils/options.h"
#include "utils/limits.h"
#include "version.h"

namespace karing::controllers {

void health_controller::health(const drogon::HttpRequestPtr& req, std::function<void(const drogon::HttpResponsePtr&)>&& cb) {
  const auto& options = karing::options::current();
  Json::Value out;
  out["status"] = "ok";
  out["version"] = KARING_VERSION;
  out["limit"] = options.limit;
  Json::Value size(Json::objectValue);
  size["file"] = std::to_string(options.max_file_bytes / karing::limits::kBytesPerMb) + "MB";
  size["text"] = std::to_string(options.max_text_bytes / karing::limits::kBytesPerMb) + "MB";
  out["size"] = size;
  Json::Value path(Json::objectValue);
  path["db"] = options.db_path;
  path["upload"] = options.upload_path;
  path["log"] = options.log_path;
  out["path"] = path;
  Json::Value listener(Json::objectValue);
  listener["address"] = options.listen_address;
  listener["port"] = options.port;
  out["listener"] = listener;
  if (const auto info = karing::db::inspect::read_health_info(options.db_path)) {
    Json::Value db(Json::objectValue);
    db["active_items"] = info->active_items;
    db["max_items"] = info->max_items;
    db["next_id"] = info->next_id;
    out["db"] = db;
  }
  auto resp = drogon::HttpResponse::newHttpJsonResponse(out);
  resp->setStatusCode(drogon::k200OK);
  cb(resp);
}

}

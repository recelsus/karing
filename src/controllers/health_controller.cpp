#include "health_controller.h"
#include <drogon/drogon.h>
#include "utils/options.h"
#include "utils/limits.h"
#include "version.h"

namespace karing::controllers {

void health_controller::health(const drogon::HttpRequestPtr& req, std::function<void(const drogon::HttpResponsePtr&)>&& cb) {
  Json::Value out;
  out["status"] = "ok";
  out["version"] = KARING_VERSION;
  out["db_path"] = karing::options::db_path();
  out["limit_build"] = karing::options::build_limit();
  out["limit_runtime"] = karing::options::runtime_limit();
  out["limit_max"] = KARING_MAX_LIMIT;
  // sizes
  Json::Value sizes(Json::objectValue);
  sizes["max_file_bytes"] = karing::options::max_file_bytes();
  sizes["max_text_bytes"] = karing::options::max_text_bytes();
  sizes["hard_file_bytes"] = KARING_HARD_MAX_FILE_BYTES;
  sizes["hard_text_bytes"] = KARING_HARD_MAX_TEXT_BYTES;
  try {
    auto cfg = drogon::app().getCustomConfig();
    if (cfg.isMember("client_max_body_size") && cfg["client_max_body_size"].isInt64()) {
      sizes["client_max_body_size"] = cfg["client_max_body_size"].asInt64();
    }
  } catch (...) {}
  out["sizes"] = sizes;
  // tls
  Json::Value tls(Json::objectValue);
  tls["enabled"] = karing::options::tls_enabled();
  tls["require"] = karing::options::tls_require();
  tls["https_port"] = karing::options::tls_https_port();
  tls["http_port"] = karing::options::tls_http_port();
  if (karing::options::tls_enabled()) tls["cert"] = karing::options::tls_cert_path();
  out["tls"] = tls;
  out["base_path"] = karing::options::base_path();
  auto resp = drogon::HttpResponse::newHttpJsonResponse(out);
  resp->setStatusCode(drogon::k200OK);
  cb(resp);
}

}

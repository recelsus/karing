#include "health_controller.h"
#include <drogon/drogon.h>
#include "utils/options.h"
#include "version.h"

namespace karing::controllers {

void health_controller::health(const drogon::HttpRequestPtr& req, std::function<void(const drogon::HttpResponsePtr&)>&& cb) {
  auto& options_state = karing::options::runtime_options::instance();
  Json::Value out;
  out["status"] = "ok";
  out["version"] = KARING_VERSION;
  out["db_path"] = options_state.db_path();
  out["limit_build"] = options_state.build_limit();
  out["limit_runtime"] = options_state.runtime_limit();
  out["limit_max"] = KARING_MAX_LIMIT;
  // HTTP request body limit
  Json::Value sizes(Json::objectValue);
  auto extract_sizes = [&](const Json::Value& cfg) {
    if (cfg.isMember("storage") && cfg["storage"].isObject() &&
        cfg["storage"].isMember("upload_limit") && cfg["storage"]["upload_limit"].isInt64()) {
      sizes["upload_limit"] = cfg["storage"]["upload_limit"].asInt64();
    }
    if (cfg.isMember("client_max_body_size") && cfg["client_max_body_size"].isInt64()) {
      sizes["client_max_body_size"] = cfg["client_max_body_size"].asInt64();
    }
  };
  try {
    auto cfg = drogon::app().getCustomConfig();
    extract_sizes(cfg);
  } catch (...) {}
  out["sizes"] = sizes;
  // tls
  Json::Value tls(Json::objectValue);
  tls["enabled"] = options_state.tls_enabled();
  tls["require"] = options_state.tls_require();
  tls["https_port"] = options_state.tls_https_port();
  tls["http_port"] = options_state.tls_http_port();
  if (options_state.tls_enabled()) tls["cert"] = options_state.tls_cert_path();
  out["tls"] = tls;
  out["base_url"] = options_state.base_path();
  out["base_path"] = options_state.base_path();
  auto resp = drogon::HttpResponse::newHttpJsonResponse(out);
  resp->setStatusCode(drogon::k200OK);
  cb(resp);
}

}

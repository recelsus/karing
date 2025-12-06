#include "web_controller.h"
#include <drogon/drogon.h>
#include "utils/options.h"

namespace karing::controllers {

void web_controller::web_root(const drogon::HttpRequestPtr&, std::function<void(const drogon::HttpResponsePtr&)>&& cb) {
  auto& options_state = karing::options::runtime_options::instance();
  if (!options_state.web_enabled()) {
    auto resp = drogon::HttpResponse::newHttpResponse();
    resp->setStatusCode(drogon::k404NotFound);
    cb(resp);
    return;
  }
  Json::Value body;
  body["success"] = true;
  body["message"] = "Web UI placeholder";
  body["detail"] = "Static bundle not yet available";
  auto resp = drogon::HttpResponse::newHttpJsonResponse(body);
  resp->setStatusCode(drogon::k200OK);
  cb(resp);
}

}

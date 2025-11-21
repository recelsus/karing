#include "web_controller.h"
#include <drogon/drogon.h>
#include <string>

namespace karing::controllers {

void web_controller::web_root(const drogon::HttpRequestPtr&, std::function<void(const drogon::HttpResponsePtr&)>&& cb) {
  Json::Value body;
  body["success"] = true;
  body["message"] = "Web UI placeholder";
  body["detail"] = "Static bundle not yet available";
  auto resp = drogon::HttpResponse::newHttpJsonResponse(body);
  resp->setStatusCode(drogon::k200OK);
  cb(resp);
}

}

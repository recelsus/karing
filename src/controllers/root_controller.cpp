#include "root_controller.h"
#include <drogon/drogon.h>
#include "utils/options.h"
#include "utils/json_response.h"
#include "version.h"

namespace karing::controllers {

void root_controller::root(const drogon::HttpRequestPtr& req, std::function<void(const drogon::HttpResponsePtr&)>&& cb) {
  Json::Value data;
  data["service"] = "karing";
  data["version"] = KARING_VERSION;
  data["base_path"] = karing::options::base_path();
  return cb(karing::http::ok(data));
}

}

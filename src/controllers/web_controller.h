#pragma once
#include <drogon/HttpController.h>

namespace karing::controllers {

class web_controller : public drogon::HttpController<web_controller> {
 public:
  METHOD_LIST_BEGIN
  ADD_METHOD_TO(web_controller::web_root, "/web", drogon::Get, "karing::filters::https_redirect_filter", "karing::filters::hsts_filter");
  METHOD_LIST_END

  void web_root(const drogon::HttpRequestPtr& req, std::function<void(const drogon::HttpResponsePtr&)>&& cb);
};

}

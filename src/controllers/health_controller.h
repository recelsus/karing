#pragma once
#include <drogon/HttpController.h>

namespace karing::controllers {

class health_controller : public drogon::HttpController<health_controller> {
 public:
  METHOD_LIST_BEGIN
  ADD_METHOD_TO(health_controller::health, "/health", drogon::Get, "karing::filters::https_redirect_filter", "karing::filters::hsts_filter");
  METHOD_LIST_END

  void health(const drogon::HttpRequestPtr& req, std::function<void(const drogon::HttpResponsePtr&)>&& cb);
};

}


#pragma once
#include <drogon/HttpController.h>

namespace karing::controllers {

class root_controller : public drogon::HttpController<root_controller> {
 public:
  METHOD_LIST_BEGIN
  METHOD_LIST_END

  // Not exposed by default; kept for potential future static info endpoint
  void root(const drogon::HttpRequestPtr& req, std::function<void(const drogon::HttpResponsePtr&)>&& cb);
};

}

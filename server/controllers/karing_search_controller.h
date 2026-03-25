#pragma once
#include <drogon/HttpController.h>

namespace karing::controllers {

class karing_search_controller : public drogon::HttpController<karing_search_controller> {
 public:
  METHOD_LIST_BEGIN
  ADD_METHOD_TO(karing_search_controller::search, "/search", drogon::Get);
  METHOD_LIST_END

  void search(const drogon::HttpRequestPtr& req, std::function<void(const drogon::HttpResponsePtr&)>&& cb);
};

}

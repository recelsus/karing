#pragma once
#include <drogon/HttpController.h>

namespace karing::controllers {

class karing_search_live_controller : public drogon::HttpController<karing_search_live_controller> {
 public:
  METHOD_LIST_BEGIN
  ADD_METHOD_TO(karing_search_live_controller::search_live, "/search/live", drogon::Get);
  METHOD_LIST_END

  void search_live(const drogon::HttpRequestPtr& req, std::function<void(const drogon::HttpResponsePtr&)>&& cb);
};

}

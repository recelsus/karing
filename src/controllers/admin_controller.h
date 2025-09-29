#pragma once
#include <drogon/HttpController.h>

namespace karing::controllers {

class admin_controller : public drogon::HttpController<admin_controller> {
 public:
  METHOD_LIST_BEGIN
  // Admin-only listing of auth settings (API keys and IP rules)
  ADD_METHOD_TO(admin_controller::list_auth, "/admin/auth", drogon::Get,
                "karing::filters::https_redirect_filter",
                "karing::filters::hsts_filter",
                "karing::filters::auth_filter",
                "karing::filters::admin_filter");
  METHOD_LIST_END

  void list_auth(const drogon::HttpRequestPtr& req,
                 std::function<void(const drogon::HttpResponsePtr&)>&& cb);
};

}


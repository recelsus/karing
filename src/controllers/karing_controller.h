#pragma once
#include <drogon/HttpController.h>

namespace karing::controllers {

class karing_controller : public drogon::HttpController<karing_controller> {
 public:
  METHOD_LIST_BEGIN
  // Primary endpoints at root only
  ADD_METHOD_TO(karing_controller::get_karing, "/", drogon::Get, "karing::filters::https_redirect_filter", "karing::filters::hsts_filter", "karing::filters::auth_filter");
  ADD_METHOD_TO(karing_controller::post_karing, "/", drogon::Post, "karing::filters::https_redirect_filter", "karing::filters::hsts_filter", "karing::filters::auth_filter");
  ADD_METHOD_TO(karing_controller::put_karing, "/", drogon::Put, "karing::filters::https_redirect_filter", "karing::filters::hsts_filter", "karing::filters::auth_filter");
  ADD_METHOD_TO(karing_controller::patch_karing, "/", drogon::Patch, "karing::filters::https_redirect_filter", "karing::filters::hsts_filter", "karing::filters::auth_filter");
  ADD_METHOD_TO(karing_controller::delete_karing, "/", drogon::Delete, "karing::filters::https_redirect_filter", "karing::filters::hsts_filter", "karing::filters::auth_filter");
  // Search endpoint (GET/POST JSON)
  ADD_METHOD_TO(karing_controller::search, "/search", drogon::Get, "karing::filters::https_redirect_filter", "karing::filters::hsts_filter", "karing::filters::auth_filter");
  ADD_METHOD_TO(karing_controller::search, "/search", drogon::Post, "karing::filters::https_redirect_filter", "karing::filters::hsts_filter", "karing::filters::auth_filter");
  METHOD_LIST_END

  void get_karing(const drogon::HttpRequestPtr& req, std::function<void(const drogon::HttpResponsePtr&)>&& cb);
  void post_karing(const drogon::HttpRequestPtr& req, std::function<void(const drogon::HttpResponsePtr&)>&& cb);
  void put_karing(const drogon::HttpRequestPtr& req, std::function<void(const drogon::HttpResponsePtr&)>&& cb);
  void patch_karing(const drogon::HttpRequestPtr& req, std::function<void(const drogon::HttpResponsePtr&)>&& cb);
  void delete_karing(const drogon::HttpRequestPtr& req, std::function<void(const drogon::HttpResponsePtr&)>&& cb);
  void health(const drogon::HttpRequestPtr& req, std::function<void(const drogon::HttpResponsePtr&)>&& cb);
  void search(const drogon::HttpRequestPtr& req, std::function<void(const drogon::HttpResponsePtr&)>&& cb);
};

}

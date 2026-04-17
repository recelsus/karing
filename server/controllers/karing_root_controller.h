#pragma once
#include <drogon/HttpController.h>

namespace karing::controllers {

class karing_root_controller : public drogon::HttpController<karing_root_controller> {
 public:
  METHOD_LIST_BEGIN
  ADD_METHOD_TO(karing_root_controller::get_karing, "/", drogon::Get);
  ADD_METHOD_TO(karing_root_controller::post_karing, "/", drogon::Post);
  ADD_METHOD_TO(karing_root_controller::swap_karing, "/swap", drogon::Post);
  ADD_METHOD_TO(karing_root_controller::resequence_karing, "/resequence", drogon::Post);
  ADD_METHOD_TO(karing_root_controller::put_karing, "/", drogon::Put);
  ADD_METHOD_TO(karing_root_controller::patch_karing, "/", drogon::Patch);
  ADD_METHOD_TO(karing_root_controller::delete_karing, "/", drogon::Delete);
  METHOD_LIST_END

  void get_karing(const drogon::HttpRequestPtr& req, std::function<void(const drogon::HttpResponsePtr&)>&& cb);
  void post_karing(const drogon::HttpRequestPtr& req, std::function<void(const drogon::HttpResponsePtr&)>&& cb);
  void swap_karing(const drogon::HttpRequestPtr& req, std::function<void(const drogon::HttpResponsePtr&)>&& cb);
  void resequence_karing(const drogon::HttpRequestPtr& req, std::function<void(const drogon::HttpResponsePtr&)>&& cb);
  void put_karing(const drogon::HttpRequestPtr& req, std::function<void(const drogon::HttpResponsePtr&)>&& cb);
  void patch_karing(const drogon::HttpRequestPtr& req, std::function<void(const drogon::HttpResponsePtr&)>&& cb);
  void delete_karing(const drogon::HttpRequestPtr& req, std::function<void(const drogon::HttpResponsePtr&)>&& cb);
};

}

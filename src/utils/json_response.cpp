#include "json_response.h"

namespace karing::http {

using drogon::HttpResponse;
using drogon::HttpResponsePtr;
using drogon::HttpStatusCode;

HttpResponsePtr ok(Json::Value data, Json::Value meta) {
  Json::Value root;
  root["success"] = true;
  root["message"] = "OK";
  root["data"] = std::move(data);
  if (!meta.isNull()) root["meta"] = std::move(meta);
  auto resp = HttpResponse::newHttpJsonResponse(root);
  resp->setStatusCode(HttpStatusCode::k200OK);
  return resp;
}

HttpResponsePtr created(int id) {
  Json::Value root; root["success"] = true; root["message"] = "Created"; root["id"] = id;
  auto resp = HttpResponse::newHttpJsonResponse(root);
  resp->setStatusCode(HttpStatusCode::k201Created);
  return resp;
}

HttpResponsePtr error(HttpStatusCode status,
                      const std::string& code,
                      const std::string& message,
                      Json::Value details) {
  Json::Value root;
  root["success"] = false;
  root["code"] = code;
  root["message"] = message;
  if (!details.isNull()) root["details"] = std::move(details);
  auto resp = HttpResponse::newHttpJsonResponse(root);
  resp->setStatusCode(status);
  return resp;
}

}

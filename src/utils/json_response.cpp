#include "json_response.h"

namespace karing::http {

using drogon::HttpResponse;
using drogon::HttpResponsePtr;
using drogon::HttpStatusCode;

HttpResponsePtr ok(Json::Value data, Json::Value meta) {
  Json::Value root;
  root["data"] = std::move(data);
  if (!meta.isNull()) root["meta"] = std::move(meta);
  root["error"] = Json::nullValue;
  auto resp = HttpResponse::newHttpJsonResponse(root);
  resp->setStatusCode(HttpStatusCode::k200OK);
  return resp;
}

HttpResponsePtr created(int id) {
  Json::Value root; root["id"] = id; root["error"] = Json::nullValue;
  auto resp = HttpResponse::newHttpJsonResponse(root);
  resp->setStatusCode(HttpStatusCode::k201Created);
  return resp;
}

HttpResponsePtr error(HttpStatusCode status,
                      const std::string& code,
                      const std::string& message,
                      Json::Value details) {
  Json::Value root;
  Json::Value err;
  err["code"] = code;
  err["message"] = message;
  if (!details.isNull()) err["details"] = std::move(details);
  root["error"] = std::move(err);
  auto resp = HttpResponse::newHttpJsonResponse(root);
  resp->setStatusCode(status);
  return resp;
}

}


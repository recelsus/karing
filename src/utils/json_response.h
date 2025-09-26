#pragma once
#include <drogon/HttpResponse.h>
#include <json/json.h>
#include <string>

namespace karing::http {

// Standard JSON success response: { data, meta?, error: null }
drogon::HttpResponsePtr ok(Json::Value data, Json::Value meta = Json::nullValue);

// 201 Created: { id, error: null }
drogon::HttpResponsePtr created(int id);

// Standard JSON error: { error: { code, message, details? } }
drogon::HttpResponsePtr error(drogon::HttpStatusCode status,
                              const std::string& code,
                              const std::string& message,
                              Json::Value details = Json::nullValue);

}


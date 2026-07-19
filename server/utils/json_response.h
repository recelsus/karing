#pragma once
#include <drogon/HttpResponse.h>
#include <json/json.h>
#include <string>

#include "common/error/app_error.h"

namespace karing::http {

// Standard JSON success response: { success: true, message: "OK", data, meta? }
drogon::HttpResponsePtr ok(Json::Value data, Json::Value meta = Json::nullValue);

// 201 Created: { success: true, message: "Created", id }
drogon::HttpResponsePtr created(int id);

// Standard JSON error: { success: false, code, message, details? }
drogon::HttpResponsePtr error(drogon::HttpStatusCode status,
                              const std::string& code,
                              const std::string& message,
                              Json::Value details = Json::nullValue);

drogon::HttpResponsePtr error(drogon::HttpStatusCode status,
                              const karing::domain::app_error& error,
                              bool include_detail = false);

}

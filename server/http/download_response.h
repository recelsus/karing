#pragma once

#include <string>

#include <drogon/drogon.h>

#include "dao/karing_dao.h"

namespace karing::http {

bool is_downloadable_text_record(const karing::dao::KaringRecord& record);

drogon::HttpResponsePtr make_text_response(const std::string& body);
drogon::HttpResponsePtr make_text_blob_response(const std::string& mime, std::string body);
drogon::HttpResponsePtr make_file_response(const std::string& mime,
                                           const std::string& filename,
                                           std::string body,
                                           bool attachment);

}

#include "http/download_response.h"

#include <iomanip>
#include <sstream>
#include <string_view>

namespace karing::http {

namespace {

bool starts_with(std::string_view value, std::string_view prefix) {
  return value.rfind(prefix, 0) == 0;
}

std::string ascii_fallback_filename(std::string_view filename) {
  std::string out;
  out.reserve(filename.size());
  for (unsigned char ch : filename) {
    if (ch >= 0x20 && ch <= 0x7e && ch != '"' && ch != '\\' && ch != ';') out.push_back(static_cast<char>(ch));
    else out.push_back('_');
  }
  if (out.empty()) return "download";
  return out;
}

std::string percent_encode_utf8(std::string_view value) {
  std::ostringstream out;
  out << std::uppercase << std::hex;
  for (unsigned char ch : value) {
    if ((ch >= 'A' && ch <= 'Z') ||
        (ch >= 'a' && ch <= 'z') ||
        (ch >= '0' && ch <= '9') ||
        ch == '-' || ch == '.' || ch == '_' || ch == '~') {
      out << static_cast<char>(ch);
    } else {
      out << '%' << std::setw(2) << std::setfill('0') << static_cast<int>(ch);
    }
  }
  return out.str();
}

std::string content_disposition(std::string_view disposition, std::string_view filename) {
  const auto fallback = ascii_fallback_filename(filename);
  return std::string(disposition) +
         "; filename=\"" + fallback + "\"" +
         "; filename*=UTF-8''" + percent_encode_utf8(filename);
}

}  // namespace

bool is_downloadable_text_record(const karing::dao::KaringRecord& record) {
  return !record.is_file && !record.filename.empty() && starts_with(record.mime, "text/");
}

drogon::HttpResponsePtr make_text_response(const std::string& body) {
  auto resp = drogon::HttpResponse::newHttpResponse();
  resp->setStatusCode(drogon::k200OK);
  resp->setContentTypeCode(drogon::CT_TEXT_PLAIN);
  resp->addHeader("Content-Type", "text/plain; charset=utf-8");
  resp->setBody(body);
  return resp;
}

drogon::HttpResponsePtr make_text_blob_response(const std::string& mime, std::string body) {
  auto resp = drogon::HttpResponse::newHttpResponse();
  resp->setStatusCode(drogon::k200OK);
  resp->setContentTypeCode(drogon::CT_CUSTOM);
  resp->setContentTypeString(mime.empty() ? "text/plain; charset=utf-8" : mime);
  resp->setBody(std::move(body));
  return resp;
}

drogon::HttpResponsePtr make_file_response(const std::string& mime,
                                           const std::string& filename,
                                           std::string body,
                                           bool attachment) {
  auto resp = drogon::HttpResponse::newHttpResponse();
  resp->setStatusCode(drogon::k200OK);
  resp->setContentTypeCode(drogon::CT_CUSTOM);
  resp->setContentTypeString(mime);
  resp->addHeader("Content-Disposition",
                  content_disposition(attachment ? "attachment" : "inline", filename));
  resp->setBody(std::move(body));
  return resp;
}

}

#include "karing_root_controller.h"

#include <drogon/MultiPart.h>
#include <drogon/drogon.h>
#include <cctype>
#include <iomanip>
#include <sstream>
#include <optional>
#include <string>

#include "dao/karing_dao.h"
#include "utils/json_response.h"
#include "utils/options.h"
#include "utils/upload_mime.h"

using drogon::HttpRequestPtr;
using drogon::HttpResponsePtr;
using drogon::HttpStatusCode;

namespace karing::controllers {

namespace {

bool starts_with(const std::string& s, const std::string& prefix) {
  return s.rfind(prefix, 0) == 0;
}

bool is_downloadable_text_record(const dao::KaringRecord& record) {
  return !record.is_file && !record.filename.empty() && starts_with(record.mime, "text/");
}

std::optional<int> parse_id_param(const drogon::SafeStringMap<std::string>& params, const char* key) {
  const auto it = params.find(key);
  if (it == params.end()) return std::nullopt;
  try {
    return std::stoi(it->second);
  } catch (...) {
    return std::nullopt;
  }
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

Json::Value record_to_json(const dao::KaringRecord& r) {
  Json::Value j;
  j["id"] = r.id;
  j["is_file"] = r.is_file;
  if (!r.is_file && !r.content.empty()) j["content"] = r.content;
  if (!r.filename.empty()) j["filename"] = r.filename;
  if (!r.mime.empty()) j["mime"] = r.mime;
  j["created_at"] = Json::Int64(r.created_at);
  if (r.updated_at) j["updated_at"] = Json::Int64(*r.updated_at);
  return j;
}

}  // namespace

void karing_root_controller::get_karing(const HttpRequestPtr& req, std::function<void(const HttpResponsePtr&)>&& cb) {
  const auto& options = karing::options::current();
  dao::KaringDao dao(options.db_path, options.upload_path);
  auto params = req->getParameters();
  const bool want_json = (params.find("json") != params.end() && params.at("json") == "true");

  if (want_json) {
    if (params.find("id") != params.end()) {
      const auto id = parse_id_param(params, "id");
      if (!id.has_value()) return cb(karing::http::error(HttpStatusCode::k400BadRequest, "E_VALIDATION", "Invalid id"));
      auto rec = dao.get_by_id(*id);
      if (!rec) return cb(karing::http::error(HttpStatusCode::k404NotFound, "E_NOT_FOUND", "Not found"));
      Json::Value data = Json::arrayValue;
      data.append(record_to_json(*rec));
      return cb(karing::http::ok(data));
    }
    auto lid = dao.latest_id();
    if (!lid) return cb(karing::http::error(HttpStatusCode::k404NotFound, "E_NOT_FOUND", "No content"));
    auto rec = dao.get_by_id(*lid);
    if (!rec) return cb(karing::http::error(HttpStatusCode::k404NotFound, "E_NOT_FOUND", "Not found"));
    Json::Value data = Json::arrayValue;
    data.append(record_to_json(*rec));
    return cb(karing::http::ok(data));
  }

  if (params.empty()) {
    auto lid = dao.latest_id();
    if (!lid) return cb(karing::http::error(HttpStatusCode::k404NotFound, "E_NOT_FOUND", "No content"));
    auto rec = dao.get_by_id(*lid);
    if (!rec) return cb(karing::http::error(HttpStatusCode::k404NotFound, "E_NOT_FOUND", "Not found"));
    if (!rec->is_file) {
      if (is_downloadable_text_record(*rec)) {
        std::string mime, filename, data;
        if (!dao.get_file_blob(rec->id, mime, filename, data)) return cb(karing::http::error(HttpStatusCode::k404NotFound, "E_NOT_FOUND", "File not found"));
        auto resp = drogon::HttpResponse::newHttpResponse();
        resp->setStatusCode(HttpStatusCode::k200OK);
        resp->setContentTypeCode(drogon::CT_CUSTOM);
        resp->setContentTypeString(mime.empty() ? "text/plain; charset=utf-8" : mime);
        resp->setBody(std::move(data));
        return cb(resp);
      }
      auto resp = drogon::HttpResponse::newHttpResponse();
      resp->setStatusCode(HttpStatusCode::k200OK);
      resp->setContentTypeCode(drogon::CT_TEXT_PLAIN);
      resp->addHeader("Content-Type", "text/plain; charset=utf-8");
      resp->setBody(rec->content);
      return cb(resp);
    }
    std::string mime, filename, data;
    if (!dao.get_file_blob(rec->id, mime, filename, data)) return cb(karing::http::error(HttpStatusCode::k404NotFound, "E_NOT_FOUND", "File not found"));
    auto resp = drogon::HttpResponse::newHttpResponse();
    resp->setStatusCode(HttpStatusCode::k200OK);
    resp->setContentTypeCode(drogon::CT_CUSTOM);
    resp->setContentTypeString(mime);
    resp->addHeader("Content-Disposition", content_disposition("inline", filename));
    resp->setBody(std::move(data));
    return cb(resp);
  }

  if (params.find("id") != params.end()) {
    const auto id = parse_id_param(params, "id");
    if (!id.has_value()) return cb(karing::http::error(HttpStatusCode::k400BadRequest, "E_VALIDATION", "Invalid id"));
    if (params.find("as") != params.end() && params.at("as") == "download") {
      std::string mime, filename, data;
      if (!dao.get_file_blob(*id, mime, filename, data)) return cb(karing::http::error(HttpStatusCode::k404NotFound, "E_NOT_FOUND", "File not found"));
      auto resp = drogon::HttpResponse::newHttpResponse();
      resp->setStatusCode(HttpStatusCode::k200OK);
      resp->setContentTypeCode(drogon::CT_CUSTOM);
      resp->setContentTypeString(mime);
      resp->addHeader("Content-Disposition", content_disposition("attachment", filename));
      resp->setBody(std::move(data));
      return cb(resp);
    }
    auto rec = dao.get_by_id(*id);
    if (!rec) return cb(karing::http::error(HttpStatusCode::k404NotFound, "E_NOT_FOUND", "Not found"));
    if (!rec->is_file) {
      if (is_downloadable_text_record(*rec)) {
        std::string mime, filename, data;
        if (!dao.get_file_blob(rec->id, mime, filename, data)) return cb(karing::http::error(HttpStatusCode::k404NotFound, "E_NOT_FOUND", "File not found"));
        auto resp = drogon::HttpResponse::newHttpResponse();
        resp->setStatusCode(HttpStatusCode::k200OK);
        resp->setContentTypeCode(drogon::CT_CUSTOM);
        resp->setContentTypeString(mime.empty() ? "text/plain; charset=utf-8" : mime);
        resp->setBody(std::move(data));
        return cb(resp);
      }
      auto resp = drogon::HttpResponse::newHttpResponse();
      resp->setStatusCode(HttpStatusCode::k200OK);
      resp->setContentTypeCode(drogon::CT_TEXT_PLAIN);
      resp->addHeader("Content-Type", "text/plain; charset=utf-8");
      resp->setBody(rec->content);
      return cb(resp);
    }
    std::string mime, filename, data;
    if (!dao.get_file_blob(rec->id, mime, filename, data)) return cb(karing::http::error(HttpStatusCode::k404NotFound, "E_NOT_FOUND", "File not found"));
    auto resp = drogon::HttpResponse::newHttpResponse();
    resp->setStatusCode(HttpStatusCode::k200OK);
    resp->setContentTypeCode(drogon::CT_CUSTOM);
    resp->setContentTypeString(mime);
    resp->addHeader("Content-Disposition", content_disposition("inline", filename));
    resp->setBody(std::move(data));
    return cb(resp);
  }

  return cb(karing::http::error(HttpStatusCode::k400BadRequest, "E_QUERY", "Unsupported query on root path"));
}

void karing_root_controller::post_karing(const HttpRequestPtr& req, std::function<void(const HttpResponsePtr&)>&& cb) {
  const auto& options = karing::options::current();
  dao::KaringDao dao(options.db_path, options.upload_path);
  const auto& ctype = req->getHeader("content-type");

  if (ctype.find("application/json") != std::string::npos) {
    auto json = req->getJsonObject();
    if (!json || !(*json)["content"].isString()) return cb(karing::http::error(HttpStatusCode::k400BadRequest, "E_VALIDATION", "Content required"));
    std::string content = (*json)["content"].asString();
    if (static_cast<long long>(content.size()) > static_cast<long long>(options.max_text_bytes)) {
      return cb(karing::http::error(HttpStatusCode::k413RequestEntityTooLarge, "E_SIZE", "Text too large"));
    }
    int id = dao.insert_text(content);
    if (id < 0) return cb(karing::http::error(HttpStatusCode::k500InternalServerError, "E_INTERNAL", "Insert failed"));
    return cb(karing::http::created(id));
  }

  if (ctype.find("multipart/form-data") != std::string::npos) {
    drogon::MultiPartParser mpp;
    if (mpp.parse(req) != 0) return cb(karing::http::error(HttpStatusCode::k400BadRequest, "E_VALIDATION", "Multipart parse error"));
    const auto& files = mpp.getFiles();
    if (files.empty()) return cb(karing::http::error(HttpStatusCode::k400BadRequest, "E_VALIDATION", "File required"));
    const auto& f = files.front();
    std::string filename = mpp.getParameter<std::string>("filename");
    if (filename.empty()) filename = f.getFileName();
    std::string mime = karing::upload_mime::normalise(mpp.getParameter<std::string>("mime"), filename);
    if (mime.empty()) mime = "application/octet-stream";
    if (!karing::upload_mime::is_supported(mime)) return cb(karing::http::error(HttpStatusCode::k415UnsupportedMediaType, "E_MIME", "Unsupported media type"));
    std::string data(f.fileData(), f.fileLength());
    if (static_cast<long long>(data.size()) > static_cast<long long>(options.max_file_bytes)) {
      return cb(karing::http::error(HttpStatusCode::k413RequestEntityTooLarge, "E_SIZE", "File too large"));
    }
    int id = dao.insert_file(filename, mime, data);
    if (id < 0) return cb(karing::http::error(HttpStatusCode::k500InternalServerError, "E_INTERNAL", "Insert failed"));
    return cb(karing::http::created(id));
  }

  return cb(karing::http::error(HttpStatusCode::k415UnsupportedMediaType, "E_MIME", "Unsupported content-type"));
}

void karing_root_controller::swap_karing(const HttpRequestPtr& req, std::function<void(const HttpResponsePtr&)>&& cb) {
  const auto& options = karing::options::current();
  dao::KaringDao dao(options.db_path, options.upload_path);
  const auto params = req->getParameters();

  if (params.find("id1") == params.end() || params.find("id2") == params.end()) {
    return cb(karing::http::error(HttpStatusCode::k400BadRequest, "E_VALIDATION", "id1 and id2 are required"));
  }

  int id1 = 0;
  int id2 = 0;
  const auto id1_value = parse_id_param(params, "id1");
  const auto id2_value = parse_id_param(params, "id2");
  if (!id1_value.has_value() || !id2_value.has_value()) {
    return cb(karing::http::error(HttpStatusCode::k400BadRequest, "E_VALIDATION", "id1 and id2 must be integers"));
  }
  id1 = *id1_value;
  id2 = *id2_value;

  if (id1 == id2) {
    return cb(karing::http::error(HttpStatusCode::k400BadRequest, "E_VALIDATION", "id1 and id2 must be different"));
  }

  if (!dao.swap_entries(id1, id2)) {
    return cb(karing::http::error(HttpStatusCode::k404NotFound, "E_NOT_FOUND", "Swap failed"));
  }

  const auto first = dao.get_by_id(id1);
  const auto second = dao.get_by_id(id2);
  if (!first || !second) {
    return cb(karing::http::error(HttpStatusCode::k500InternalServerError, "E_INTERNAL", "Swap completed but records could not be reloaded"));
  }

  Json::Value out = Json::arrayValue;
  out.append(record_to_json(*first));
  out.append(record_to_json(*second));
  return cb(karing::http::ok(out));
}

void karing_root_controller::put_karing(const HttpRequestPtr& req, std::function<void(const HttpResponsePtr&)>&& cb) {
  const auto& options = karing::options::current();
  dao::KaringDao dao(options.db_path, options.upload_path);
  auto params = req->getParameters();
  if (params.find("id") == params.end()) return cb(karing::http::error(HttpStatusCode::k400BadRequest, "E_VALIDATION", "Id required"));
  const auto id = parse_id_param(params, "id");
  if (!id.has_value()) return cb(karing::http::error(HttpStatusCode::k400BadRequest, "E_VALIDATION", "Invalid id"));
  const auto& ctype = req->getHeader("content-type");

  if (ctype.find("application/json") != std::string::npos) {
    auto json = req->getJsonObject();
    if (!json || !(*json)["content"].isString()) return cb(karing::http::error(HttpStatusCode::k400BadRequest, "E_VALIDATION", "Content required"));
    std::string content = (*json)["content"].asString();
    if (static_cast<long long>(content.size()) > static_cast<long long>(options.max_text_bytes)) return cb(karing::http::error(HttpStatusCode::k413RequestEntityTooLarge, "E_SIZE", "Text too large"));
    if (!dao.update_text(*id, content)) return cb(karing::http::error(HttpStatusCode::k404NotFound, "E_NOT_FOUND", "Update failed"));
    Json::Value out;
    out["id"] = *id;
    return cb(karing::http::ok(out));
  }

  if (ctype.find("multipart/form-data") != std::string::npos) {
    drogon::MultiPartParser mpp;
    if (mpp.parse(req) != 0) return cb(karing::http::error(HttpStatusCode::k400BadRequest, "E_VALIDATION", "Multipart parse error"));
    const auto& files = mpp.getFiles();
    if (files.empty()) return cb(karing::http::error(HttpStatusCode::k400BadRequest, "E_VALIDATION", "File required"));
    const auto& f = files.front();
    std::string filename = mpp.getParameter<std::string>("filename");
    if (filename.empty()) filename = f.getFileName();
    std::string mime = karing::upload_mime::normalise(mpp.getParameter<std::string>("mime"), filename);
    if (mime.empty()) mime = "application/octet-stream";
    if (!karing::upload_mime::is_supported(mime)) return cb(karing::http::error(HttpStatusCode::k415UnsupportedMediaType, "E_MIME", "Unsupported media type"));
    std::string data(f.fileData(), f.fileLength());
    if (static_cast<long long>(data.size()) > static_cast<long long>(options.max_file_bytes)) return cb(karing::http::error(HttpStatusCode::k413RequestEntityTooLarge, "E_SIZE", "File too large"));
    if (!dao.update_file(*id, filename, mime, data)) return cb(karing::http::error(HttpStatusCode::k404NotFound, "E_NOT_FOUND", "Update failed"));
    Json::Value out;
    out["id"] = *id;
    return cb(karing::http::ok(out));
  }

  return cb(karing::http::error(HttpStatusCode::k415UnsupportedMediaType, "E_MIME", "Unsupported content-type"));
}

void karing_root_controller::patch_karing(const HttpRequestPtr& req, std::function<void(const HttpResponsePtr&)>&& cb) {
  const auto& options = karing::options::current();
  dao::KaringDao dao(options.db_path, options.upload_path);
  auto params = req->getParameters();
  if (params.find("id") == params.end()) return cb(karing::http::error(HttpStatusCode::k400BadRequest, "E_VALIDATION", "Id required"));
  const auto id = parse_id_param(params, "id");
  if (!id.has_value()) return cb(karing::http::error(HttpStatusCode::k400BadRequest, "E_VALIDATION", "Invalid id"));
  const auto& ctype = req->getHeader("content-type");

  if (ctype.find("application/json") != std::string::npos) {
    auto json = req->getJsonObject();
    if (!json) return cb(karing::http::error(HttpStatusCode::k400BadRequest, "E_VALIDATION", "JSON required"));
    std::optional<std::string> content;
    if ((*json)["content"].isString()) {
      content = (*json)["content"].asString();
      if (static_cast<long long>(content->size()) > static_cast<long long>(options.max_text_bytes)) return cb(karing::http::error(HttpStatusCode::k413RequestEntityTooLarge, "E_SIZE", "Text too large"));
    }
    if (!dao.patch_text(*id, content)) return cb(karing::http::error(HttpStatusCode::k409Conflict, "E_CONFLICT", "Patch failed"));
    Json::Value out;
    out["id"] = *id;
    return cb(karing::http::ok(out));
  }

  if (ctype.find("multipart/form-data") != std::string::npos) {
    drogon::MultiPartParser mpp;
    if (mpp.parse(req) != 0) return cb(karing::http::error(HttpStatusCode::k400BadRequest, "E_VALIDATION", "Multipart parse error"));
    const auto& files = mpp.getFiles();
    std::optional<std::string> data;
    if (!files.empty()) {
      const auto& f = files.front();
      data = std::string(f.fileData(), f.fileLength());
      if (static_cast<long long>(data->size()) > static_cast<long long>(options.max_file_bytes)) return cb(karing::http::error(HttpStatusCode::k413RequestEntityTooLarge, "E_SIZE", "File too large"));
    }
    std::optional<std::string> filename;
    std::optional<std::string> mime;
    if (auto p = mpp.getParameter<std::string>("filename"); !p.empty()) filename = p;
    if (auto p = mpp.getParameter<std::string>("mime"); !p.empty()) {
      const auto normalised = karing::upload_mime::normalise(p, filename.value_or(files.empty() ? std::string{} : files.front().getFileName()));
      if (normalised.empty() || !karing::upload_mime::is_supported(normalised)) {
        return cb(karing::http::error(HttpStatusCode::k415UnsupportedMediaType, "E_MIME", "Unsupported media type"));
      }
      mime = normalised;
    } else if (!files.empty()) {
      const auto guessed = karing::upload_mime::normalise("", filename.value_or(files.front().getFileName()));
      if (!guessed.empty()) mime = guessed;
    }
    if (!dao.patch_file(*id, filename, mime, data)) return cb(karing::http::error(HttpStatusCode::k409Conflict, "E_CONFLICT", "Patch failed"));
    Json::Value out;
    out["id"] = *id;
    return cb(karing::http::ok(out));
  }

  return cb(karing::http::error(HttpStatusCode::k415UnsupportedMediaType, "E_MIME", "Unsupported content-type"));
}

void karing_root_controller::delete_karing(const HttpRequestPtr& req, std::function<void(const HttpResponsePtr&)>&& cb) {
  const auto& options = karing::options::current();
  dao::KaringDao dao(options.db_path, options.upload_path);
  auto params = req->getParameters();
  if (params.find("id") == params.end()) {
    if (!dao.logical_delete_latest_recent(600)) {
      return cb(karing::http::error(HttpStatusCode::k404NotFound, "E_NOT_FOUND", "No recent latest record to delete"));
    }
  } else {
    const auto id = parse_id_param(params, "id");
    if (!id.has_value()) return cb(karing::http::error(HttpStatusCode::k400BadRequest, "E_VALIDATION", "Invalid id"));
    if (!dao.logical_delete(*id)) return cb(karing::http::error(HttpStatusCode::k404NotFound, "E_NOT_FOUND", "Not found"));
  }
  auto resp = drogon::HttpResponse::newHttpResponse();
  resp->setStatusCode(HttpStatusCode::k204NoContent);
  cb(resp);
}

}  // namespace karing::controllers

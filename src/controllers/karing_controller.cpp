#include "karing_controller.h"
#include <drogon/HttpAppFramework.h>
#include <drogon/MultiPart.h>
#include <drogon/drogon.h>
#include <drogon/utils/Utilities.h>
#include <string>
#include <optional>
#include <fstream>
#include "dao/karing_dao.h"
#include "utils/options.h"
#include "utils/limits.h"
#include "utils/json_response.h"
#include "utils/search_query.h"
#include "version.h"
#include "utils/cursor.h"

using drogon::HttpRequestPtr;
using drogon::HttpResponsePtr;
using drogon::HttpResponse;
using drogon::HttpStatusCode;

namespace karing::controllers {

namespace {
HttpResponsePtr json_ok(Json::Value v) {
  auto resp = drogon::HttpResponse::newHttpJsonResponse(v);
  resp->setStatusCode(HttpStatusCode::k200OK);
  return resp;
}
HttpResponsePtr json_created(Json::Value v) {
  auto resp = drogon::HttpResponse::newHttpJsonResponse(v);
  resp->setStatusCode(HttpStatusCode::k201Created);
  return resp;
}
HttpResponsePtr json_err(HttpStatusCode code, const std::string& msg) {
  Json::Value v;
  v["error"] = msg;
  auto resp = drogon::HttpResponse::newHttpJsonResponse(v);
  resp->setStatusCode(code);
  return resp;
}

Json::Value record_to_json(const dao::KaringRecord& r) {
  Json::Value j;
  j["id"] = r.id;
  j["is_file"] = r.is_file;
  if (!r.key.empty()) j["key"] = r.key;
  if (!r.is_file) {
    if (!r.content.empty()) j["content"] = r.content;
  } else {
    if (!r.filename.empty()) j["filename"] = r.filename;
    if (!r.mime.empty()) j["mime"] = r.mime;
  }
  j["created_at"] = Json::Int64(r.created_at);
  if (r.updated_at) j["updated_at"] = Json::Int64(*r.updated_at);
  return j;
}
}

void karing_controller::get_karing(const HttpRequestPtr& req, std::function<void(const HttpResponsePtr&)>&& cb) {
  dao::KaringDao dao(options::db_path());
  auto params = req->getParameters();
  // id=... (single) or id+as=download handled first
  if (params.find("id") != params.end()) {
    int id = std::stoi(params.at("id"));
    if (params.find("as") != params.end() && params.at("as") == "download") {
      std::string mime, filename, data;
      if (!dao.get_file_blob(id, mime, filename, data)) return cb(karing::http::error(HttpStatusCode::k404NotFound, "E_NOT_FOUND", "File not found"));
      auto resp = drogon::HttpResponse::newHttpResponse();
      resp->setStatusCode(HttpStatusCode::k200OK);
      resp->setContentTypeCode(drogon::CT_CUSTOM);
      resp->setContentTypeString(mime);
      resp->addHeader("Content-Disposition", std::string("attachment; filename=\"") + filename + "\"");
      resp->setBody(std::move(data));
      return cb(resp);
    }
    auto rec = dao.get_by_id(id);
    if (!rec) return cb(karing::http::error(HttpStatusCode::k404NotFound, "E_NOT_FOUND", "Not found"));
    Json::Value data = Json::arrayValue; data.append(record_to_json(*rec));
    return cb(karing::http::ok(data));
  }
  // search q=... (FTS on text)
  bool hasLimit = params.find("limit") != params.end();
  int offset = 0;
  if (params.find("offset") != params.end()) {
    try { offset = std::max(0, std::stoi(params.at("offset"))); } catch (...) { offset = 0; }
  }
  // Cursor takes precedence over offset if present
  std::optional<karing::cursor::Cursor> cur;
  if (params.find("cursor") != params.end() && !params.at("cursor").empty()) {
    cur = karing::cursor::parse(params.at("cursor"));
  }
  int lim = 1; // default for non-search path: single latest
  if (hasLimit) {
    int ql = std::max(1, std::stoi(params.at("limit")));
    lim = std::min(ql, options::runtime_limit());
  }
  if (params.find("q") != params.end() && !params.at("q").empty()) {
    // For search, if limit is not specified, default to runtime limit
    if (!hasLimit) lim = options::runtime_limit();
    auto qb = karing::search::build_fts_query(params.at("q"));
    if (qb.err) {
      Json::Value det; det["reason"] = *qb.err;
      return cb(karing::http::error(HttpStatusCode::k400BadRequest, "E_QUERY", "Invalid search query", det));
    }
    std::vector<dao::KaringRecord> list;
    if (cur) {
      if (!dao.try_search_fts_after(qb.fts, lim, cur->created_at, cur->id, list)) {
        list = dao.search_text_like_after(params.at("q"), lim, cur->created_at, cur->id);
      }
    } else if (!dao.try_search_fts(qb.fts, lim, offset, list)) {
      // fallback to LIKE for environments without proper FTS behaviour
      list = dao.search_text_like(params.at("q"), lim, offset);
    }
    Json::Value data = Json::arrayValue; for (auto& r : list) data.append(record_to_json(r));
    Json::Value meta; meta["count"] = (int)list.size(); meta["limit"] = lim; meta["offset"] = offset; meta["has_more"] = (int)list.size() == lim;
    if (cur) meta["cursor"] = params.at("cursor");
    if (meta["has_more"].asBool()) {
      const auto& last = list.back();
      meta["next_cursor"] = karing::cursor::build(last.created_at, last.id);
      meta["next_offset"] = offset + (int)list.size();
    }
    // total for search
    long long total = 0;
    if (!dao.count_search_fts(qb.fts, total)) total = dao.count_search_like(params.at("q"));
    meta["total"] = Json::Int64(total);
    return cb(karing::http::ok(data, meta));
  }
  // default: latest (single by default)
  // Apply simple filters when no search term
  dao::KaringDao::Filters filt;
  filt.include_inactive = (params.find("all") != params.end() && params.at("all") == "true");
  if (auto it = params.find("key"); it != params.end() && !it->second.empty()) filt.key = it->second;
  if (auto it = params.find("is_file"); it != params.end()) {
    if (it->second == "true") filt.is_file = 1; else if (it->second == "false") filt.is_file = 0;
  }
  if (auto it = params.find("mime"); it != params.end() && !it->second.empty()) filt.mime = it->second;
  if (auto it = params.find("filename"); it != params.end() && !it->second.empty()) filt.filename = it->second;
  if (auto it = params.find("order"); it != params.end()) filt.order_desc = !(it->second == "asc");

  std::vector<dao::KaringRecord> list;
  if (cur) {
    // cursor paging applies only for the default listing (without filters) for now
    list = dao.list_latest_after(lim, cur->created_at, cur->id);
  } else if (filt.key || filt.is_file || filt.mime || filt.filename || filt.include_inactive || !filt.order_desc) {
    list = dao.list_filtered(lim, offset, filt);
  } else {
    list = dao.list_latest(lim, offset);
  }
  Json::Value data = Json::arrayValue; for (auto& r : list) data.append(record_to_json(r));
  Json::Value meta; meta["count"] = (int)list.size(); meta["limit"] = lim; meta["offset"] = offset; meta["has_more"] = (int)list.size() == lim;
  if (cur) meta["cursor"] = params.count("cursor") ? params.at("cursor") : "";
  if (meta["has_more"].asBool()) {
    const auto& last = list.back();
    meta["next_cursor"] = karing::cursor::build(last.created_at, last.id);
    meta["next_offset"] = offset + (int)list.size();
  }
  meta["total"] = (Json::Int64)(filt.key || filt.is_file || filt.mime || filt.filename || filt.include_inactive || !filt.order_desc
    ? dao.count_filtered(filt)
    : dao.count_active());
  return cb(karing::http::ok(data, meta));
}

void karing_controller::post_karing(const HttpRequestPtr& req, std::function<void(const HttpResponsePtr&)>&& cb) {
  dao::KaringDao dao(options::db_path());
  const auto& ctype = req->getHeader("content-type");
  std::string client_ip = req->getPeerAddr().toIp();

  if (ctype.find("application/json") != std::string::npos) {
    auto json = req->getJsonObject();
    if (!json || !(*json)["content"].isString()) return cb(karing::http::error(HttpStatusCode::k400BadRequest, "E_VALIDATION", "Content required"));
    std::string content = (*json)["content"].asString();
    const auto maxText = karing::options::max_text_bytes();
    if ((long long)content.size() > (long long)maxText) {
      return cb(karing::http::error(HttpStatusCode::k413RequestEntityTooLarge, "E_SIZE", "Text too large"));
    }
    std::string syntax = (*json)["syntax"].isString() ? (*json)["syntax"].asString() : "";
    std::string key = (*json)["key"].isString() ? (*json)["key"].asString() : "";
    int id = dao.insert_text(content, syntax, key, client_ip, std::nullopt);
    if (id < 0) return cb(karing::http::error(HttpStatusCode::k500InternalServerError, "E_INTERNAL", "Insert failed"));
    return cb(karing::http::created(id));
  }

  if (ctype.find("multipart/form-data") != std::string::npos) {
    drogon::MultiPartParser mpp;
    if (mpp.parse(req) != 0) return cb(karing::http::error(HttpStatusCode::k400BadRequest, "E_VALIDATION", "Multipart parse error"));
    const auto& files = mpp.getFiles();
    if (files.empty()) return cb(karing::http::error(HttpStatusCode::k400BadRequest, "E_VALIDATION", "File required"));
    const auto& f = files.front();
    std::string mime = req->getParameter("mime");
    if (mime.empty()) mime = "application/octet-stream";
    // Allow images and audio at minimum
    auto starts_with = [](const std::string& s, const std::string& p){ return s.rfind(p, 0) == 0; };
    if (!(starts_with(mime, "image/") || starts_with(mime, "audio/"))) {
      return cb(karing::http::error(HttpStatusCode::k415UnsupportedMediaType, "E_MIME", "Unsupported media type"));
    }
    std::string filenameParam = req->getParameter("filename");
    if (filenameParam.empty()) filenameParam = f.getFileName();
    std::string key = req->getParameter("key");

    // Read file bytes
    std::string data(f.fileData(), f.fileLength());
    const auto maxFile = karing::options::max_file_bytes();
    if ((long long)data.size() > (long long)maxFile) {
      return cb(karing::http::error(HttpStatusCode::k413RequestEntityTooLarge, "E_SIZE", "File too large"));
    }

    int id = dao.insert_file(filenameParam, mime, data, key, client_ip, std::nullopt);
    if (id < 0) return cb(karing::http::error(HttpStatusCode::k500InternalServerError, "E_INTERNAL", "Insert failed"));
    return cb(karing::http::created(id));
  }

  return cb(karing::http::error(HttpStatusCode::k415UnsupportedMediaType, "E_MIME", "Unsupported content-type"));
}

void karing_controller::put_karing(const HttpRequestPtr& req, std::function<void(const HttpResponsePtr&)>&& cb) {
  dao::KaringDao dao(options::db_path());
  auto params = req->getParameters();
  if (params.find("id") == params.end()) return cb(karing::http::error(HttpStatusCode::k400BadRequest, "E_VALIDATION", "Id required"));
  int id = std::stoi(params.at("id"));
  const auto& ctype = req->getHeader("content-type");
  std::string client_ip = req->getPeerAddr().toIp();
  if (ctype.find("application/json") != std::string::npos) {
    auto json = req->getJsonObject();
    if (!json || !(*json)["content"].isString()) return cb(karing::http::error(HttpStatusCode::k400BadRequest, "E_VALIDATION", "Content required"));
    std::string content = (*json)["content"].asString();
    const auto maxText = karing::options::max_text_bytes();
    if ((long long)content.size() > (long long)maxText) return cb(karing::http::error(HttpStatusCode::k413RequestEntityTooLarge, "E_SIZE", "Text too large"));
    std::string syntax = (*json)["syntax"].isString() ? (*json)["syntax"].asString() : "";
    if (!dao.update_text(id, content, syntax, client_ip, std::nullopt)) return cb(karing::http::error(HttpStatusCode::k404NotFound, "E_NOT_FOUND", "Update failed"));
    Json::Value d; d["id"] = id; return cb(karing::http::ok(d));
  }
  if (ctype.find("multipart/form-data") != std::string::npos) {
    drogon::MultiPartParser mpp; if (mpp.parse(req) != 0) return cb(karing::http::error(HttpStatusCode::k400BadRequest, "E_VALIDATION", "Multipart parse error"));
    const auto& files = mpp.getFiles(); if (files.empty()) return cb(json_err(HttpStatusCode::k400BadRequest, "file required"));
    const auto& f = files.front();
    std::string mime = req->getParameter("mime"); if (mime.empty()) mime = "application/octet-stream";
    auto starts_with = [](const std::string& s, const std::string& p){ return s.rfind(p, 0) == 0; };
    if (!(starts_with(mime, "image/") || starts_with(mime, "audio/"))) return cb(karing::http::error(HttpStatusCode::k415UnsupportedMediaType, "E_MIME", "Unsupported media type"));
    std::string filenameParam = req->getParameter("filename"); if (filenameParam.empty()) filenameParam = f.getFileName();
    std::string data(f.fileData(), f.fileLength());
    const auto maxFile = karing::options::max_file_bytes(); if ((long long)data.size() > (long long)maxFile) return cb(karing::http::error(HttpStatusCode::k413RequestEntityTooLarge, "E_SIZE", "File too large"));
    if (!dao.update_file(id, filenameParam, mime, data, client_ip, std::nullopt)) return cb(karing::http::error(HttpStatusCode::k404NotFound, "E_NOT_FOUND", "Update failed"));
    Json::Value dj; dj["id"] = id; return cb(karing::http::ok(dj));
  }
  return cb(karing::http::error(HttpStatusCode::k415UnsupportedMediaType, "E_MIME", "Unsupported content-type"));
}

void karing_controller::patch_karing(const HttpRequestPtr& req, std::function<void(const HttpResponsePtr&)>&& cb) {
  dao::KaringDao dao(options::db_path());
  auto params = req->getParameters();
  if (params.find("id") == params.end()) return cb(karing::http::error(HttpStatusCode::k400BadRequest, "E_VALIDATION", "Id required"));
  int id = std::stoi(params.at("id"));
  const auto& ctype = req->getHeader("content-type");
  std::string client_ip = req->getPeerAddr().toIp();
  if (ctype.find("application/json") != std::string::npos) {
    auto json = req->getJsonObject(); if (!json) return cb(karing::http::error(HttpStatusCode::k400BadRequest, "E_VALIDATION", "JSON required"));
    std::optional<std::string> content, syntax;
    if ((*json)["content"].isString()) {
      content = (*json)["content"].asString();
      if ((long long)content->size() > (long long)karing::options::max_text_bytes()) return cb(karing::http::error(HttpStatusCode::k413RequestEntityTooLarge, "E_SIZE", "Text too large"));
    }
    if ((*json)["syntax"].isString()) syntax = (*json)["syntax"].asString();
    if (!dao.patch_text(id, content, syntax, client_ip, std::nullopt)) return cb(karing::http::error(HttpStatusCode::k409Conflict, "E_CONFLICT", "Patch failed"));
    Json::Value dd; dd["id"] = id; return cb(karing::http::ok(dd));
  }
  if (ctype.find("multipart/form-data") != std::string::npos) {
    drogon::MultiPartParser mpp; if (mpp.parse(req) != 0) return cb(karing::http::error(HttpStatusCode::k400BadRequest, "E_VALIDATION", "Multipart parse error"));
    const auto& files = mpp.getFiles();
    std::optional<std::string> data;
    if (!files.empty()) {
      const auto& f = files.front();
      data = std::string(f.fileData(), f.fileLength());
      if ((long long)data->size() > (long long)karing::options::max_file_bytes()) return cb(karing::http::error(HttpStatusCode::k413RequestEntityTooLarge, "E_SIZE", "File too large"));
    }
    std::optional<std::string> filename, mime;
    if (auto p = req->getParameter("filename"); !p.empty()) filename = p;
    if (auto p = req->getParameter("mime"); !p.empty()) {
      auto starts_with = [](const std::string& s, const std::string& p){ return s.rfind(p, 0) == 0; };
      if (!(starts_with(p, "image/") || starts_with(p, "audio/"))) return cb(karing::http::error(HttpStatusCode::k415UnsupportedMediaType, "E_MIME", "Unsupported media type"));
      mime = p;
    }
    if (!dao.patch_file(id, filename, mime, data, client_ip, std::nullopt)) return cb(karing::http::error(HttpStatusCode::k409Conflict, "E_CONFLICT", "Patch failed"));
    Json::Value de; de["id"] = id; return cb(karing::http::ok(de));
  }
  return cb(karing::http::error(HttpStatusCode::k415UnsupportedMediaType, "E_MIME", "Unsupported content-type"));
}

void karing_controller::delete_karing(const HttpRequestPtr& req, std::function<void(const HttpResponsePtr&)>&& cb) {
  dao::KaringDao dao(options::db_path());
  auto params = req->getParameters();
  if (params.find("id") == params.end()) return cb(karing::http::error(HttpStatusCode::k400BadRequest, "E_VALIDATION", "Id required"));
  int id = std::stoi(params.at("id"));
  if (!dao.logical_delete(id)) return cb(karing::http::error(HttpStatusCode::k404NotFound, "E_NOT_FOUND", "Not found"));
  auto resp = drogon::HttpResponse::newHttpResponse();
  resp->setStatusCode(HttpStatusCode::k204NoContent);
  cb(resp);
}

void karing_controller::restore_karing(const HttpRequestPtr& req, std::function<void(const HttpResponsePtr&)>&& cb) {
  dao::KaringDao dao(options::db_path());
  auto params = req->getParameters();
  if (params.find("id") == params.end()) return cb(karing::http::error(HttpStatusCode::k400BadRequest, "E_VALIDATION", "Id required"));
  int id = std::stoi(params.at("id"));
  if (!dao.restore_latest_snapshot(id)) return cb(karing::http::error(HttpStatusCode::k409Conflict, "E_CONFLICT", "No restorable snapshot"));
  Json::Value d; d["id"] = id; return cb(karing::http::ok(d));
}

void karing_controller::health(const HttpRequestPtr& req, std::function<void(const HttpResponsePtr&)>&& cb) {
  dao::KaringDao dao(options::db_path());
  Json::Value out;
  out["status"] = "ok";
  out["version"] = KARING_VERSION;
  out["db_path"] = options::db_path();
  out["limit_build"] = karing::options::build_limit();
  out["limit_runtime"] = karing::options::runtime_limit();
  out["limit_max"] = KARING_MAX_LIMIT;
  // sizes
  Json::Value sizes(Json::objectValue);
  sizes["max_file_bytes"] = karing::options::max_file_bytes();
  sizes["max_text_bytes"] = karing::options::max_text_bytes();
  sizes["hard_file_bytes"] = KARING_HARD_MAX_FILE_BYTES;
  sizes["hard_text_bytes"] = KARING_HARD_MAX_TEXT_BYTES;
  // client_max_body_size from drogon config if present
  try {
    auto cfg = drogon::app().getCustomConfig();
    if (cfg.isMember("client_max_body_size") && cfg["client_max_body_size"].isInt64()) {
      sizes["client_max_body_size"] = cfg["client_max_body_size"].asInt64();
    }
  } catch (...) {}
  out["sizes"] = sizes;
  // tls
  Json::Value tls(Json::objectValue);
  tls["enabled"] = karing::options::tls_enabled();
  tls["require"] = karing::options::tls_require();
  tls["https_port"] = karing::options::tls_https_port();
  tls["http_port"] = karing::options::tls_http_port();
  if (karing::options::tls_enabled()) {
    tls["cert"] = karing::options::tls_cert_path();
  }
  out["tls"] = tls;
  out["base_path"] = karing::options::base_path();
  // build info
  Json::Value build(Json::objectValue);
  build["type"] = KARING_BUILD_TYPE;
  build["cxx_standard"] = KARING_CXX_STANDARD;
  build["revision"] = KARING_GIT_REV;
  build["branch"] = KARING_GIT_BRANCH;
  build["number"] = KARING_BUILD_NUMBER;
  build["time"] = KARING_BUILD_TIME;
  build["host"] = KARING_BUILD_HOST;
  build["user"] = KARING_BUILD_USER;
  build["os"] = KARING_BUILD_OS;
  build["compiler_id"] = KARING_COMPILER_ID;
  build["compiler_version"] = KARING_COMPILER_VERSION;
  build["cmake_generator"] = KARING_CMAKE_GENERATOR;
  build["cmake_version"] = KARING_CMAKE_VERSION;
  out["build"] = build;
  out["active_count"] = dao.count_active();
  cb(json_ok(out));
}

}

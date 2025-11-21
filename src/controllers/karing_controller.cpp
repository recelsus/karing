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
Json::Value record_to_json(const dao::KaringRecord& r) {
  Json::Value j;
  j["id"] = r.id;
  j["is_file"] = r.is_file;
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

std::string to_lower_copy(std::string v) {
  std::transform(v.begin(), v.end(), v.begin(), [](unsigned char c) { return static_cast<char>(std::tolower(c)); });
  return v;
}

std::optional<int> extract_id(const HttpRequestPtr& req, const std::shared_ptr<Json::Value>& json) {
  if (auto id_param = req->getParameter("id"); !id_param.empty()) {
    try {
      return std::stoi(id_param);
    } catch (...) {}
  }
  if (json) {
    if ((*json)["id"].isInt()) return (*json)["id"].asInt();
    if ((*json)["id"].isString()) {
      try {
        return std::stoi((*json)["id"].asString());
      } catch (...) {}
    }
  }
  return std::nullopt;
}
}

void karing_controller::get_karing(const HttpRequestPtr& req, std::function<void(const HttpResponsePtr&)>&& cb) {
  auto& options_state = karing::options::runtime_options::instance();
  dao::KaringDao dao(options_state.db_path());
  auto params = req->getParameters();
  bool want_json = (params.find("json") != params.end() && params.at("json") == "true");

  // JSON mode: return JSON for latest or specific id
  if (want_json) {
    if (params.find("id") != params.end()) {
      int id = std::stoi(params.at("id"));
      auto rec = dao.get_by_id(id);
      if (!rec) return cb(karing::http::error(HttpStatusCode::k404NotFound, "E_NOT_FOUND", "Not found"));
      Json::Value data = Json::arrayValue; data.append(record_to_json(*rec));
      return cb(karing::http::ok(data));
    } else {
      auto lid = dao.latest_id();
      if (!lid) return cb(karing::http::error(HttpStatusCode::k404NotFound, "E_NOT_FOUND", "No content"));
      auto rec = dao.get_by_id(*lid);
      if (!rec) return cb(karing::http::error(HttpStatusCode::k404NotFound, "E_NOT_FOUND", "Not found"));
      Json::Value data = Json::arrayValue; data.append(record_to_json(*rec));
      return cb(karing::http::ok(data));
    }
  }
  // Raw latest response when no query params: text/plain for text, or file bytes inline
  if (params.empty()) {
    auto lid = dao.latest_id();
    if (!lid) return cb(karing::http::error(HttpStatusCode::k404NotFound, "E_NOT_FOUND", "No content"));
    auto rec = dao.get_by_id(*lid);
    if (!rec) return cb(karing::http::error(HttpStatusCode::k404NotFound, "E_NOT_FOUND", "Not found"));
    if (!rec->is_file) {
      auto resp = drogon::HttpResponse::newHttpResponse();
      resp->setStatusCode(HttpStatusCode::k200OK);
      resp->setContentTypeCode(drogon::CT_TEXT_PLAIN);
      resp->addHeader("Content-Type", "text/plain; charset=utf-8");
      resp->setBody(rec->content);
      return cb(resp);
    } else {
      std::string mime, filename, data;
      if (!dao.get_file_blob(rec->id, mime, filename, data)) return cb(karing::http::error(HttpStatusCode::k404NotFound, "E_NOT_FOUND", "File not found"));
      auto resp = drogon::HttpResponse::newHttpResponse();
      resp->setStatusCode(HttpStatusCode::k200OK);
      resp->setContentTypeCode(drogon::CT_CUSTOM);
      resp->setContentTypeString(mime);
      // inline; do not force attachment
      resp->addHeader("Content-Disposition", std::string("inline; filename=\"") + filename + "\"");
      resp->setBody(std::move(data));
      return cb(resp);
    }
  }
  // id=... (single) handled first; default is inline, 'as=download' forces attachment
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
    if (!rec->is_file) {
      auto resp = drogon::HttpResponse::newHttpResponse();
      resp->setStatusCode(HttpStatusCode::k200OK);
      resp->setContentTypeCode(drogon::CT_TEXT_PLAIN);
      resp->addHeader("Content-Type", "text/plain; charset=utf-8");
      resp->setBody(rec->content);
      return cb(resp);
    } else {
      std::string mime, filename, data;
      if (!dao.get_file_blob(rec->id, mime, filename, data)) return cb(karing::http::error(HttpStatusCode::k404NotFound, "E_NOT_FOUND", "File not found"));
      auto resp = drogon::HttpResponse::newHttpResponse();
      resp->setStatusCode(HttpStatusCode::k200OK);
      resp->setContentTypeCode(drogon::CT_CUSTOM);
      resp->setContentTypeString(mime);
      resp->addHeader("Content-Disposition", std::string("inline; filename=\"") + filename + "\"");
      resp->setBody(std::move(data));
      return cb(resp);
    }
  }
  // No other parameters supported on root path (search moved to /search)
  return cb(karing::http::error(HttpStatusCode::k400BadRequest, "E_QUERY", "Unsupported query on root path"));
}

void karing_controller::search(const HttpRequestPtr& req, std::function<void(const HttpResponsePtr&)>&& cb) {
  auto& options_state = karing::options::runtime_options::instance();
  dao::KaringDao dao(options_state.db_path());
  auto params = req->getParameters();
  // POST JSON body support
  Json::Value body; if (req->method()==drogon::Post && req->getHeader("content-type").find("application/json")!=std::string::npos) { if (auto j=req->getJsonObject()) body=*j; }
  auto get_str = [&](const char* k)->std::string{ if (body.isMember(k) && body[k].isString()) return body[k].asString(); auto it=params.find(k); return it!=params.end()? it->second : std::string(); };
  auto get_int = [&](const char* k, int def)->int{ if (body.isMember(k) && body[k].isInt()) return body[k].asInt(); auto it=params.find(k); if (it!=params.end()) { try { return std::stoi(it->second);} catch(...){} } return def; };

  int offset = 0; // offset is not supported (fixed to 0)
  int lim = get_int("limit", options_state.runtime_limit());
  lim = std::min(std::max(1, lim), options_state.runtime_limit());
  std::string q = get_str("q");
  std::optional<int> is_file; std::string ty=get_str("type"); if (ty=="text") is_file=0; else if (ty=="file") is_file=1;

  std::vector<dao::KaringRecord> list;
  Json::Value meta(Json::objectValue);
  if (!q.empty()) {
    auto qb = karing::search::build_fts_query(q);
    if (qb.err) { Json::Value det; det["reason"] = *qb.err; return cb(karing::http::error(HttpStatusCode::k400BadRequest, "E_QUERY", "Invalid search query", det)); }
    // FTS only
    bool ok=false;
    if (is_file.has_value()) {
      // in-memory filter for type
      std::vector<dao::KaringRecord> tmp; ok = dao.try_search_fts(qb.fts, lim, tmp);
      for (auto& r : tmp) if ((int)r.is_file == *is_file) list.push_back(std::move(r));
    } else {
      ok = dao.try_search_fts(qb.fts, lim, list);
    }
    if (!ok) return cb(karing::http::error(HttpStatusCode::k503ServiceUnavailable, "E_FTS_UNAVAILABLE", "Full-text search unavailable"));
    meta["count"]=(int)list.size(); meta["limit"]=lim;
  } else {
    // Latest listing
    if (is_file.has_value()) { dao::KaringDao::Filters f; f.is_file=*is_file; f.order_desc=true; list=dao.list_filtered(lim, f); meta["total"]=(Json::Int64)dao.count_filtered(f); }
    else { list = dao.list_latest(lim); meta["total"]=(Json::Int64)dao.count_active(); }
    meta["count"]=(int)list.size(); meta["limit"]=lim;
  }
  Json::Value data = Json::arrayValue; for (auto& r : list) data.append(record_to_json(r));
  return cb(karing::http::ok(data, meta));
}

void karing_controller::post_karing(const HttpRequestPtr& req, std::function<void(const HttpResponsePtr&)>&& cb) {
  auto& options_state = karing::options::runtime_options::instance();
  dao::KaringDao dao(options_state.db_path());
  const auto& ctype = req->getHeader("content-type");
  bool is_json = ctype.find("application/json") != std::string::npos;
  bool is_multipart = ctype.find("multipart/form-data") != std::string::npos;
  if (!is_json && !is_multipart) {
    return cb(karing::http::error(HttpStatusCode::k415UnsupportedMediaType, "E_MIME", "Unsupported content-type"));
  }

  std::shared_ptr<Json::Value> json;
  if (is_json) json = req->getJsonObject();

  drogon::MultiPartParser mpp;
  if (is_multipart) {
    if (mpp.parse(req) != 0) {
      return cb(karing::http::error(HttpStatusCode::k400BadRequest, "E_VALIDATION", "Multipart parse error"));
    }
  }

  std::string action = req->getParameter("action");
  if (action.empty() && json && (*json)["action"].isString()) action = (*json)["action"].asString();
  action = to_lower_copy(action);
  if (action.empty()) action = is_json ? "create_text" : "create_file";
  if (action == "create") action = is_json ? "create_text" : "create_file";
  if (action == "update") action = is_json ? "update_text" : "update_file";
  if (action == "patch") action = is_json ? "patch_text" : "patch_file";

  auto require_id = [&]() -> std::optional<int> {
    auto id = extract_id(req, json);
    if (!id) {
      cb(karing::http::error(HttpStatusCode::k400BadRequest, "E_VALIDATION", "Id required"));
    }
    return id;
  };

  auto ensure_json = [&](const std::string& err)->std::shared_ptr<Json::Value> {
    if (!json) {
      cb(karing::http::error(HttpStatusCode::k400BadRequest, "E_VALIDATION", err));
      return nullptr;
    }
    return json;
  };

  auto starts_with = [](const std::string& s, const std::string& p) { return s.rfind(p, 0) == 0; };

  if (action == "delete" || action == "remove") {
    auto id = require_id();
    if (!id) return;
    if (!dao.logical_delete(*id)) {
      return cb(karing::http::error(HttpStatusCode::k404NotFound, "E_NOT_FOUND", "Not found"));
    }
    auto resp = drogon::HttpResponse::newHttpResponse();
    resp->setStatusCode(HttpStatusCode::k204NoContent);
    return cb(resp);
  }

  if (action == "create_text") {
    auto json_body = ensure_json("Content required");
    if (!json_body) return;
    if (!(*json_body)["content"].isString()) {
      return cb(karing::http::error(HttpStatusCode::k400BadRequest, "E_VALIDATION", "Content required"));
    }
    std::string content = (*json_body)["content"].asString();
    if ((long long)content.size() > (long long)options_state.max_text_bytes()) {
      return cb(karing::http::error(HttpStatusCode::k413RequestEntityTooLarge, "E_SIZE", "Text too large"));
    }
    int id = dao.insert_text(content);
    if (id < 0) return cb(karing::http::error(HttpStatusCode::k500InternalServerError, "E_INTERNAL", "Insert failed"));
    return cb(karing::http::created(id));
  }

  if (action == "update_text") {
    auto json_body = ensure_json("Content required");
    if (!json_body) return;
    auto id = require_id();
    if (!id) return;
    if (!(*json_body)["content"].isString()) {
      return cb(karing::http::error(HttpStatusCode::k400BadRequest, "E_VALIDATION", "Content required"));
    }
    std::string content = (*json_body)["content"].asString();
    if ((long long)content.size() > (long long)options_state.max_text_bytes()) {
      return cb(karing::http::error(HttpStatusCode::k413RequestEntityTooLarge, "E_SIZE", "Text too large"));
    }
    if (!dao.update_text(*id, content)) {
      return cb(karing::http::error(HttpStatusCode::k404NotFound, "E_NOT_FOUND", "Update failed"));
    }
    Json::Value data; data["id"] = *id; return cb(karing::http::ok(data));
  }

  if (action == "patch_text") {
    auto json_body = ensure_json("JSON required");
    if (!json_body) return;
    auto id = require_id();
    if (!id) return;
    std::optional<std::string> content;
    if ((*json_body)["content"].isString()) {
      content = (*json_body)["content"].asString();
      if ((long long)content->size() > (long long)options_state.max_text_bytes()) {
        return cb(karing::http::error(HttpStatusCode::k413RequestEntityTooLarge, "E_SIZE", "Text too large"));
      }
    }
    if (!dao.patch_text(*id, content)) {
      return cb(karing::http::error(HttpStatusCode::k409Conflict, "E_CONFLICT", "Patch failed"));
    }
    Json::Value data; data["id"] = *id; return cb(karing::http::ok(data));
  }

  if (action == "create_file" || action == "update_file" || action == "patch_file") {
    if (!is_multipart) {
      return cb(karing::http::error(HttpStatusCode::k415UnsupportedMediaType, "E_MIME", "Multipart required"));
    }
    const auto& files = mpp.getFiles();
    const drogon::HttpFile* file_part = files.empty() ? nullptr : &files.front();
    if ((action == "create_file" || action == "update_file") && !file_part) {
      return cb(karing::http::error(HttpStatusCode::k400BadRequest, "E_VALIDATION", "File required"));
    }
    std::optional<int> id;
    if (action != "create_file") {
      id = require_id();
      if (!id) return;
    }
    std::optional<std::string> data;
    if (file_part) {
      data = std::string(file_part->fileData(), file_part->fileLength());
      if ((long long)data->size() > (long long)options_state.max_file_bytes()) {
        return cb(karing::http::error(HttpStatusCode::k413RequestEntityTooLarge, "E_SIZE", "File too large"));
      }
    }
    std::string mime = req->getParameter("mime");
    if (!mime.empty()) {
      if (!(starts_with(mime, "image/") || starts_with(mime, "audio/"))) {
        return cb(karing::http::error(HttpStatusCode::k415UnsupportedMediaType, "E_MIME", "Unsupported media type"));
      }
    } else if (file_part) {
      mime = "application/octet-stream";
    }
    std::string filename = req->getParameter("filename");
    if (filename.empty() && file_part) filename = file_part->getFileName();
    std::optional<std::string> filename_opt;
    if (!filename.empty()) filename_opt = filename;
    std::optional<std::string> mime_opt;
    if (!mime.empty()) mime_opt = mime;
    if (action == "create_file") {
      if (!data) return cb(karing::http::error(HttpStatusCode::k400BadRequest, "E_VALIDATION", "File required"));
      if (!mime_opt) mime_opt = "application/octet-stream";
      std::string mime_value = *mime_opt;
      std::string filename_value = filename_opt.value_or(file_part ? file_part->getFileName() : "upload");
      int new_id = dao.insert_file(filename_value, mime_value, *data);
      if (new_id < 0) return cb(karing::http::error(HttpStatusCode::k500InternalServerError, "E_INTERNAL", "Insert failed"));
      return cb(karing::http::created(new_id));
    }
    if (action == "update_file") {
      if (!data || !mime_opt) {
        return cb(karing::http::error(HttpStatusCode::k400BadRequest, "E_VALIDATION", "File and mime required"));
      }
      if (!dao.update_file(*id, filename_opt.value_or(file_part ? file_part->getFileName() : "upload"), *mime_opt, *data)) {
        return cb(karing::http::error(HttpStatusCode::k404NotFound, "E_NOT_FOUND", "Update failed"));
      }
      Json::Value data_json; data_json["id"] = *id; return cb(karing::http::ok(data_json));
    }
    if (!dao.patch_file(*id, filename_opt, mime_opt, data)) {
      return cb(karing::http::error(HttpStatusCode::k409Conflict, "E_CONFLICT", "Patch failed"));
    }
    Json::Value data_json; data_json["id"] = *id; return cb(karing::http::ok(data_json));
  }

  return cb(karing::http::error(HttpStatusCode::k400BadRequest, "E_ACTION", "Unsupported action"));
}

// restore endpoint removed

void karing_controller::health(const HttpRequestPtr& req, std::function<void(const HttpResponsePtr&)>&& cb) {
  auto& options_state = karing::options::runtime_options::instance();
  dao::KaringDao dao(options_state.db_path());
  Json::Value out;
  out["status"] = "ok";
  out["version"] = KARING_VERSION;
  out["db_path"] = options_state.db_path();
  out["limit_build"] = options_state.build_limit();
  out["limit_runtime"] = options_state.runtime_limit();
  out["limit_max"] = KARING_MAX_LIMIT;
  // sizes
  Json::Value sizes(Json::objectValue);
  sizes["max_file_bytes"] = options_state.max_file_bytes();
  sizes["max_text_bytes"] = options_state.max_text_bytes();
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
  tls["enabled"] = options_state.tls_enabled();
  tls["require"] = options_state.tls_require();
  tls["https_port"] = options_state.tls_https_port();
  tls["http_port"] = options_state.tls_http_port();
  if (options_state.tls_enabled()) {
    tls["cert"] = options_state.tls_cert_path();
  }
  out["tls"] = tls;
  out["base_path"] = options_state.base_path();
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

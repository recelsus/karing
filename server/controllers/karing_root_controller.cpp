#include "karing_root_controller.h"

#include <drogon/MultiPart.h>
#include <drogon/drogon.h>
#include <optional>
#include <string>

#include "http/download_response.h"
#include "http/record_json.h"
#include "http/request_params.h"
#include "services/root_service.h"
#include "utils/json_response.h"
#include "utils/options.h"
#include "utils/upload_mime.h"

using drogon::HttpRequestPtr;
using drogon::HttpResponsePtr;
using drogon::HttpStatusCode;

namespace karing::controllers {

namespace {

services::root_service make_root_service() {
  const auto& options = karing::options::current();
  return services::root_service(options.db_path, options.upload_path);
}

}  // namespace

void karing_root_controller::get_karing(const HttpRequestPtr& req, std::function<void(const HttpResponsePtr&)>&& cb) {
  const auto service = make_root_service();
  const auto params = req->getParameters();
  const bool want_json = (params.find("json") != params.end() && params.at("json") == "true");

  if (want_json) {
    if (params.find("id") != params.end()) {
      const auto id = karing::http::parse_int_param(params, "id");
      if (id.status != karing::http::int_param_status::ok) {
        return cb(karing::http::error(HttpStatusCode::k400BadRequest, "E_VALIDATION", "Invalid id"));
      }
      auto rec = service.record_by_id(id.value);
      if (!rec) return cb(karing::http::error(HttpStatusCode::k404NotFound, "E_NOT_FOUND", "Not found"));
      Json::Value data = Json::arrayValue;
      data.append(karing::http::record_to_json(*rec));
      return cb(karing::http::ok(data));
    }
    auto rec = service.latest_record();
    if (!rec) return cb(karing::http::error(HttpStatusCode::k404NotFound, "E_NOT_FOUND", "Not found"));
    Json::Value data = Json::arrayValue;
    data.append(karing::http::record_to_json(*rec));
    return cb(karing::http::ok(data));
  }

  if (params.empty()) {
    auto rec = service.latest_record();
    if (!rec) return cb(karing::http::error(HttpStatusCode::k404NotFound, "E_NOT_FOUND", "Not found"));
    if (!rec->is_file) {
      if (karing::http::is_downloadable_text_record(*rec)) {
        services::file_blob blob;
        if (!service.file_blob_by_id(rec->id, blob)) {
          return cb(karing::http::error(HttpStatusCode::k404NotFound, "E_NOT_FOUND", "File not found"));
        }
        return cb(karing::http::make_text_blob_response(blob.mime, std::move(blob.data)));
      }
      return cb(karing::http::make_text_response(rec->content));
    }
    services::file_blob blob;
    if (!service.file_blob_by_id(rec->id, blob)) {
      return cb(karing::http::error(HttpStatusCode::k404NotFound, "E_NOT_FOUND", "File not found"));
    }
    return cb(karing::http::make_file_response(blob.mime, blob.filename, std::move(blob.data), false));
  }

  if (params.find("id") != params.end()) {
    const auto id = karing::http::parse_int_param(params, "id");
    if (id.status != karing::http::int_param_status::ok) {
      return cb(karing::http::error(HttpStatusCode::k400BadRequest, "E_VALIDATION", "Invalid id"));
    }
    if (params.find("as") != params.end() && params.at("as") == "download") {
      services::file_blob blob;
      if (!service.file_blob_by_id(id.value, blob)) {
        return cb(karing::http::error(HttpStatusCode::k404NotFound, "E_NOT_FOUND", "File not found"));
      }
      return cb(karing::http::make_file_response(blob.mime, blob.filename, std::move(blob.data), true));
    }
    auto rec = service.record_by_id(id.value);
    if (!rec) return cb(karing::http::error(HttpStatusCode::k404NotFound, "E_NOT_FOUND", "Not found"));
    if (!rec->is_file) {
      if (karing::http::is_downloadable_text_record(*rec)) {
        services::file_blob blob;
        if (!service.file_blob_by_id(rec->id, blob)) {
          return cb(karing::http::error(HttpStatusCode::k404NotFound, "E_NOT_FOUND", "File not found"));
        }
        return cb(karing::http::make_text_blob_response(blob.mime, std::move(blob.data)));
      }
      return cb(karing::http::make_text_response(rec->content));
    }
    services::file_blob blob;
    if (!service.file_blob_by_id(rec->id, blob)) {
      return cb(karing::http::error(HttpStatusCode::k404NotFound, "E_NOT_FOUND", "File not found"));
    }
    return cb(karing::http::make_file_response(blob.mime, blob.filename, std::move(blob.data), false));
  }

  return cb(karing::http::error(HttpStatusCode::k400BadRequest, "E_QUERY", "Unsupported query on root path"));
}

void karing_root_controller::post_karing(const HttpRequestPtr& req, std::function<void(const HttpResponsePtr&)>&& cb) {
  const auto& options = karing::options::current();
  const auto service = make_root_service();
  const auto& ctype = req->getHeader("content-type");

  if (ctype.find("application/json") != std::string::npos) {
    auto json = req->getJsonObject();
    if (!json || !(*json)["content"].isString()) return cb(karing::http::error(HttpStatusCode::k400BadRequest, "E_VALIDATION", "Content required"));
    std::string content = (*json)["content"].asString();
    if (static_cast<long long>(content.size()) > static_cast<long long>(options.max_text_bytes)) {
      return cb(karing::http::error(HttpStatusCode::k413RequestEntityTooLarge, "E_SIZE", "Text too large"));
    }
    int id = service.create_text(content);
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
    int id = service.create_file(filename, mime, data);
    if (id < 0) return cb(karing::http::error(HttpStatusCode::k500InternalServerError, "E_INTERNAL", "Insert failed"));
    return cb(karing::http::created(id));
  }

  return cb(karing::http::error(HttpStatusCode::k415UnsupportedMediaType, "E_MIME", "Unsupported content-type"));
}

void karing_root_controller::swap_karing(const HttpRequestPtr& req, std::function<void(const HttpResponsePtr&)>&& cb) {
  const auto service = make_root_service();
  const auto params = req->getParameters();

  const auto id1 = karing::http::parse_int_param(params, "id1");
  const auto id2 = karing::http::parse_int_param(params, "id2");
  if (id1.status == karing::http::int_param_status::missing || id2.status == karing::http::int_param_status::missing) {
    return cb(karing::http::error(HttpStatusCode::k400BadRequest, "E_VALIDATION", "id1 and id2 are required"));
  }
  if (id1.status != karing::http::int_param_status::ok || id2.status != karing::http::int_param_status::ok) {
    return cb(karing::http::error(HttpStatusCode::k400BadRequest, "E_VALIDATION", "id1 and id2 must be integers"));
  }

  if (id1.value == id2.value) {
    return cb(karing::http::error(HttpStatusCode::k400BadRequest, "E_VALIDATION", "id1 and id2 must be different"));
  }

  const auto swapped = service.swap(id1.value, id2.value);
  if (!swapped) {
    return cb(karing::http::error(HttpStatusCode::k404NotFound, "E_NOT_FOUND", "Swap failed"));
  }

  Json::Value out = Json::arrayValue;
  out.append(karing::http::record_to_json(swapped->first));
  out.append(karing::http::record_to_json(swapped->second));
  return cb(karing::http::ok(out));
}

void karing_root_controller::resequence_karing(const HttpRequestPtr&,
                                               std::function<void(const HttpResponsePtr&)>&& cb) {
  const auto service = make_root_service();
  const auto resequenced = service.resequence();
  if (!resequenced) {
    return cb(karing::http::error(HttpStatusCode::k500InternalServerError, "E_INTERNAL", "Resequence failed"));
  }

  Json::Value out = Json::arrayValue;
  for (const auto& record : resequenced->first) {
    out.append(karing::http::record_to_json(record));
  }
  Json::Value meta(Json::objectValue);
  meta["count"] = static_cast<int>(resequenced->first.size());
  meta["next_id"] = resequenced->second;
  return cb(karing::http::ok(out, meta));
}

void karing_root_controller::put_karing(const HttpRequestPtr& req, std::function<void(const HttpResponsePtr&)>&& cb) {
  const auto& options = karing::options::current();
  const auto service = make_root_service();
  const auto params = req->getParameters();
  const auto id = karing::http::parse_int_param(params, "id");
  if (id.status == karing::http::int_param_status::missing) return cb(karing::http::error(HttpStatusCode::k400BadRequest, "E_VALIDATION", "Id required"));
  if (id.status != karing::http::int_param_status::ok) return cb(karing::http::error(HttpStatusCode::k400BadRequest, "E_VALIDATION", "Invalid id"));
  const auto& ctype = req->getHeader("content-type");

  if (ctype.find("application/json") != std::string::npos) {
    auto json = req->getJsonObject();
    if (!json || !(*json)["content"].isString()) return cb(karing::http::error(HttpStatusCode::k400BadRequest, "E_VALIDATION", "Content required"));
    std::string content = (*json)["content"].asString();
    if (static_cast<long long>(content.size()) > static_cast<long long>(options.max_text_bytes)) return cb(karing::http::error(HttpStatusCode::k413RequestEntityTooLarge, "E_SIZE", "Text too large"));
    if (!service.replace_text(id.value, content)) return cb(karing::http::error(HttpStatusCode::k404NotFound, "E_NOT_FOUND", "Update failed"));
    Json::Value out;
    out["id"] = id.value;
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
    if (!service.replace_file(id.value, filename, mime, data)) return cb(karing::http::error(HttpStatusCode::k404NotFound, "E_NOT_FOUND", "Update failed"));
    Json::Value out;
    out["id"] = id.value;
    return cb(karing::http::ok(out));
  }

  return cb(karing::http::error(HttpStatusCode::k415UnsupportedMediaType, "E_MIME", "Unsupported content-type"));
}

void karing_root_controller::patch_karing(const HttpRequestPtr& req, std::function<void(const HttpResponsePtr&)>&& cb) {
  const auto& options = karing::options::current();
  const auto service = make_root_service();
  const auto params = req->getParameters();
  const auto id = karing::http::parse_int_param(params, "id");
  if (id.status == karing::http::int_param_status::missing) return cb(karing::http::error(HttpStatusCode::k400BadRequest, "E_VALIDATION", "Id required"));
  if (id.status != karing::http::int_param_status::ok) return cb(karing::http::error(HttpStatusCode::k400BadRequest, "E_VALIDATION", "Invalid id"));
  const auto& ctype = req->getHeader("content-type");

  if (ctype.find("application/json") != std::string::npos) {
    auto json = req->getJsonObject();
    if (!json) return cb(karing::http::error(HttpStatusCode::k400BadRequest, "E_VALIDATION", "JSON required"));
    std::optional<std::string> content;
    if ((*json)["content"].isString()) {
      content = (*json)["content"].asString();
      if (static_cast<long long>(content->size()) > static_cast<long long>(options.max_text_bytes)) return cb(karing::http::error(HttpStatusCode::k413RequestEntityTooLarge, "E_SIZE", "Text too large"));
    }
    if (!service.patch_text(id.value, content)) return cb(karing::http::error(HttpStatusCode::k409Conflict, "E_CONFLICT", "Patch failed"));
    Json::Value out;
    out["id"] = id.value;
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
    if (!service.patch_file(id.value, filename, mime, data)) return cb(karing::http::error(HttpStatusCode::k409Conflict, "E_CONFLICT", "Patch failed"));
    Json::Value out;
    out["id"] = id.value;
    return cb(karing::http::ok(out));
  }

  return cb(karing::http::error(HttpStatusCode::k415UnsupportedMediaType, "E_MIME", "Unsupported content-type"));
}

void karing_root_controller::delete_karing(const HttpRequestPtr& req, std::function<void(const HttpResponsePtr&)>&& cb) {
  const auto service = make_root_service();
  const auto params = req->getParameters();
  if (params.find("id") == params.end()) {
    if (!service.delete_latest_recent(600)) {
      return cb(karing::http::error(HttpStatusCode::k404NotFound, "E_NOT_FOUND", "No recent latest record to delete"));
    }
  } else {
    const auto id = karing::http::parse_int_param(params, "id");
    if (id.status != karing::http::int_param_status::ok) return cb(karing::http::error(HttpStatusCode::k400BadRequest, "E_VALIDATION", "Invalid id"));
    if (!service.delete_by_id(id.value)) return cb(karing::http::error(HttpStatusCode::k404NotFound, "E_NOT_FOUND", "Not found"));
  }
  auto resp = drogon::HttpResponse::newHttpResponse();
  resp->setStatusCode(HttpStatusCode::k204NoContent);
  cb(resp);
}

}  // namespace karing::controllers

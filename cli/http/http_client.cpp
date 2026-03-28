#include "http/http_client.h"

#include <curl/curl.h>

#include <memory>
#include <stdexcept>
#include <string>

#include "utils/auth_cache.h"

namespace karing::cli::http {

namespace {

struct curl_global_guard {
  curl_global_guard() { curl_global_init(CURL_GLOBAL_DEFAULT); }
  ~curl_global_guard() { curl_global_cleanup(); }
};

curl_global_guard kCurlGlobalGuard;

size_t write_body(char* ptr, size_t size, size_t nmemb, void* userdata) {
  auto* out = static_cast<std::string*>(userdata);
  out->append(ptr, size * nmemb);
  return size * nmemb;
}

size_t write_header(char* buffer, size_t size, size_t nitems, void* userdata) {
  auto* out = static_cast<response*>(userdata);
  const std::string line(buffer, size * nitems);
  const std::string prefix = "Content-Type:";
  if (line.rfind(prefix, 0) == 0) {
    std::string value = line.substr(prefix.size());
    while (!value.empty() && (value.front() == ' ' || value.front() == '\t')) value.erase(value.begin());
    while (!value.empty() && (value.back() == '\r' || value.back() == '\n')) value.pop_back();
    out->content_type = value;
  }
  return size * nitems;
}

using curl_ptr = std::unique_ptr<CURL, decltype(&curl_easy_cleanup)>;
using slist_ptr = std::unique_ptr<curl_slist, decltype(&curl_slist_free_all)>;
using mime_ptr = std::unique_ptr<curl_mime, decltype(&curl_mime_free)>;

std::string encode_component(CURL* curl, const std::string& value) {
  char* raw = curl_easy_escape(curl, value.c_str(), static_cast<int>(value.size()));
  if (!raw) return value;
  std::string out = raw;
  curl_free(raw);
  return out;
}

std::string build_url(CURL* curl,
                      const std::string& base_url,
                      const std::string& path,
                      const std::map<std::string, std::string>& query) {
  std::string out = base_url + path;
  if (query.empty()) return out;
  out += '?';
  bool first = true;
  for (const auto& [key, value] : query) {
    if (!first) out += '&';
    first = false;
    out += encode_component(curl, key);
    out += '=';
    out += encode_component(curl, value);
  }
  return out;
}

response perform(CURL* curl) {
  response out;
  curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, write_body);
  curl_easy_setopt(curl, CURLOPT_WRITEDATA, &out.body);
  curl_easy_setopt(curl, CURLOPT_HEADERFUNCTION, write_header);
  curl_easy_setopt(curl, CURLOPT_HEADERDATA, &out);
  curl_easy_setopt(curl, CURLOPT_FOLLOWLOCATION, 1L);
  curl_easy_setopt(curl, CURLOPT_USERAGENT, "karing-cli/0.2.0");
  const CURLcode code = curl_easy_perform(curl);
  if (code != CURLE_OK) {
    throw std::runtime_error(std::string("curl request failed: ") + curl_easy_strerror(code));
  }
  curl_easy_getinfo(curl, CURLINFO_RESPONSE_CODE, &out.status);
  return out;
}

void apply_auth_header(struct curl_slist*& raw_headers,
                       const std::string& api_key,
                       utils::auth_scheme scheme) {
  if (scheme == utils::auth_scheme::bearer) {
    raw_headers = curl_slist_append(raw_headers, ("Authorization: Bearer " + api_key).c_str());
  } else {
    raw_headers = curl_slist_append(raw_headers, ("X-API-Key: " + api_key).c_str());
  }
}

template <typename PrepareFn>
response perform_with_optional_auth(const std::string& base_url,
                                    const std::optional<std::string>& api_key,
                                    PrepareFn&& prepare) {
  auto attempt = [&](std::optional<utils::auth_scheme> scheme) -> response {
    curl_ptr curl(curl_easy_init(), curl_easy_cleanup);
    if (!curl) throw std::runtime_error("failed to initialize curl");
    struct curl_slist* raw_headers = nullptr;
    if (api_key.has_value() && scheme.has_value()) {
      apply_auth_header(raw_headers, *api_key, *scheme);
    }
    slist_ptr headers(raw_headers, curl_slist_free_all);
    prepare(curl.get(), headers);
    return perform(curl.get());
  };

  if (!api_key.has_value()) return attempt(std::nullopt);

  const auto cached = utils::load_auth_scheme(base_url);
  if (cached.has_value()) {
    auto resp = attempt(cached);
    if (resp.status != 401 && resp.status != 403) return resp;
  }

  auto resp = attempt(utils::auth_scheme::bearer);
  if (resp.status != 401 && resp.status != 403) {
    utils::save_auth_scheme(base_url, utils::auth_scheme::bearer);
    return resp;
  }

  auto fallback = attempt(utils::auth_scheme::x_api_key);
  if (fallback.status != 401 && fallback.status != 403) {
    utils::save_auth_scheme(base_url, utils::auth_scheme::x_api_key);
  }
  return fallback;
}

}  // namespace

response get(const std::string& base_url,
             const std::string& path,
             const std::map<std::string, std::string>& query,
             const std::optional<std::string>& api_key) {
  return perform_with_optional_auth(base_url, api_key, [&](CURL* curl, const slist_ptr& headers) {
    const auto url = build_url(curl, base_url, path, query);
    curl_easy_setopt(curl, CURLOPT_URL, url.c_str());
    if (headers) curl_easy_setopt(curl, CURLOPT_HTTPHEADER, headers.get());
  });
}

response del(const std::string& base_url,
             const std::string& path,
             const std::map<std::string, std::string>& query,
             const std::optional<std::string>& api_key) {
  return perform_with_optional_auth(base_url, api_key, [&](CURL* curl, const slist_ptr& headers) {
    const auto url = build_url(curl, base_url, path, query);
    curl_easy_setopt(curl, CURLOPT_URL, url.c_str());
    curl_easy_setopt(curl, CURLOPT_CUSTOMREQUEST, "DELETE");
    if (headers) curl_easy_setopt(curl, CURLOPT_HTTPHEADER, headers.get());
  });
}

response post_json(const std::string& base_url,
                   const std::string& path,
                   const std::string& json_body,
                   const std::optional<std::string>& api_key) {
  return perform_with_optional_auth(base_url, api_key, [&](CURL* curl, const slist_ptr& auth_headers) {
    struct curl_slist* raw = nullptr;
    raw = curl_slist_append(raw, "Content-Type: application/json");
    for (auto* p = auth_headers.get(); p != nullptr; p = p->next) raw = curl_slist_append(raw, p->data);
    slist_ptr headers(raw, curl_slist_free_all);
    const auto url = build_url(curl, base_url, path, {});
    curl_easy_setopt(curl, CURLOPT_URL, url.c_str());
    curl_easy_setopt(curl, CURLOPT_POST, 1L);
    curl_easy_setopt(curl, CURLOPT_HTTPHEADER, headers.get());
    curl_easy_setopt(curl, CURLOPT_POSTFIELDS, json_body.c_str());
    curl_easy_setopt(curl, CURLOPT_POSTFIELDSIZE, static_cast<long>(json_body.size()));
    headers.release();
  });
}

response post(const std::string& base_url,
              const std::string& path,
              const std::map<std::string, std::string>& query,
              const std::optional<std::string>& api_key) {
  return perform_with_optional_auth(base_url, api_key, [&](CURL* curl, const slist_ptr& headers) {
    const auto url = build_url(curl, base_url, path, query);
    curl_easy_setopt(curl, CURLOPT_URL, url.c_str());
    curl_easy_setopt(curl, CURLOPT_POST, 1L);
    curl_easy_setopt(curl, CURLOPT_POSTFIELDS, "");
    curl_easy_setopt(curl, CURLOPT_POSTFIELDSIZE, 0L);
    if (headers) curl_easy_setopt(curl, CURLOPT_HTTPHEADER, headers.get());
  });
}

response put_json(const std::string& base_url,
                  const std::string& path,
                  const std::map<std::string, std::string>& query,
                  const std::string& json_body,
                  const std::optional<std::string>& api_key) {
  return perform_with_optional_auth(base_url, api_key, [&](CURL* curl, const slist_ptr& auth_headers) {
    struct curl_slist* raw = nullptr;
    raw = curl_slist_append(raw, "Content-Type: application/json");
    for (auto* p = auth_headers.get(); p != nullptr; p = p->next) raw = curl_slist_append(raw, p->data);
    slist_ptr headers(raw, curl_slist_free_all);
    const auto url = build_url(curl, base_url, path, query);
    curl_easy_setopt(curl, CURLOPT_URL, url.c_str());
    curl_easy_setopt(curl, CURLOPT_CUSTOMREQUEST, "PUT");
    curl_easy_setopt(curl, CURLOPT_HTTPHEADER, headers.get());
    curl_easy_setopt(curl, CURLOPT_POSTFIELDS, json_body.c_str());
    curl_easy_setopt(curl, CURLOPT_POSTFIELDSIZE, static_cast<long>(json_body.size()));
    headers.release();
  });
}

response multipart_form(const std::string& base_url,
                        const std::string& path,
                        const std::string& method,
                        const std::map<std::string, std::string>& query,
                        const std::string& file_field_name,
                        const std::string& file_path,
                        const std::optional<std::string>& filename,
                        const std::optional<std::string>& mime,
                        const std::optional<std::string>& api_key) {
  return perform_with_optional_auth(base_url, api_key, [&](CURL* curl, const slist_ptr& headers) {
    const auto url = build_url(curl, base_url, path, query);
    curl_easy_setopt(curl, CURLOPT_URL, url.c_str());
    if (method != "POST") curl_easy_setopt(curl, CURLOPT_CUSTOMREQUEST, method.c_str());
    if (headers) curl_easy_setopt(curl, CURLOPT_HTTPHEADER, headers.get());

    mime_ptr form(curl_mime_init(curl), curl_mime_free);
    if (!form) throw std::runtime_error("failed to initialize multipart form");
    curl_mimepart* part = curl_mime_addpart(form.get());
    curl_mime_name(part, file_field_name.c_str());
    curl_mime_filedata(part, file_path.c_str());
    if (filename.has_value()) curl_mime_filename(part, filename->c_str());
    if (mime.has_value()) curl_mime_type(part, mime->c_str());
    if (filename.has_value()) {
      curl_mimepart* filename_part = curl_mime_addpart(form.get());
      curl_mime_name(filename_part, "filename");
      curl_mime_data(filename_part, filename->c_str(), CURL_ZERO_TERMINATED);
    }
    if (mime.has_value()) {
      curl_mimepart* mime_part = curl_mime_addpart(form.get());
      curl_mime_name(mime_part, "mime");
      curl_mime_data(mime_part, mime->c_str(), CURL_ZERO_TERMINATED);
    }
    curl_easy_setopt(curl, CURLOPT_MIMEPOST, form.get());
    form.release();
  });
}

}

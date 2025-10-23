#include "https_redirect_filter.h"
#include <drogon/drogon.h>
#include "utils/options.h"

using drogon::HttpRequestPtr;
using drogon::HttpStatusCode;

namespace karing::filters {

void https_redirect_filter::doFilter(const HttpRequestPtr& req,
                                     drogon::FilterCallback&& fcb,
                                     drogon::FilterChainCallback&& fccb) {
  auto& options_state = karing::options::runtime_options::instance();
  if (!(options_state.tls_enabled() && options_state.tls_require())) return fccb();
  auto xfp = req->getHeader("x-forwarded-proto");
  if (!xfp.empty() && (xfp == "https" || xfp == "HTTPS")) return fccb();
  // Build https URL
  auto host = req->getHeader("host");
  int https_port = options_state.tls_https_port();
  if (host.empty()) host = "localhost";
  // Replace port in Host header if present
  auto pos = host.find(':');
  if (pos != std::string::npos) host = host.substr(0, pos);
  std::string url = std::string("https://") + host;
  if (https_port != 443 && https_port > 0) url += ":" + std::to_string(https_port);
  url += req->path();
  auto q = req->getQuery(); if (!q.empty()) { url += "?"; url += q; }
  auto resp = drogon::HttpResponse::newRedirectionResponse(url);
  resp->setStatusCode(HttpStatusCode::k301MovedPermanently);
  fcb(resp);
}

}

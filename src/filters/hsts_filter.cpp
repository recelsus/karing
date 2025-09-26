#include "hsts_filter.h"
#include <drogon/drogon.h>
#include "utils/options.h"

namespace karing::filters {

void hsts_filter::doFilter(const drogon::HttpRequestPtr& req,
                           drogon::FilterCallback&& fcb,
                           drogon::FilterChainCallback&& fccb) {
  if (!(karing::options::tls_enabled())) return fccb();
  auto xfp = req->getHeader("x-forwarded-proto");
  bool https = (!xfp.empty() && (xfp=="https"||xfp=="HTTPS"));
  if (!https) return fccb();
  auto resp = drogon::HttpResponse::newHttpResponse();
  resp->addHeader("Strict-Transport-Security", "max-age=31536000; includeSubDomains");
  return fccb();
}

}

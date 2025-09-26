#pragma once
#include <drogon/HttpFilter.h>

namespace karing::filters {

class hsts_filter : public drogon::HttpFilter<hsts_filter> {
 public:
  void doFilter(const drogon::HttpRequestPtr& req,
                drogon::FilterCallback&& fcb,
                drogon::FilterChainCallback&& fccb) override;
};

}


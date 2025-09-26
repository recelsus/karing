#pragma once
#include <drogon/HttpFilter.h>

namespace karing::filters {

class https_redirect_filter : public drogon::HttpFilter<https_redirect_filter> {
 public:
  void doFilter(const drogon::HttpRequestPtr& req,
                drogon::FilterCallback&& fcb,
                drogon::FilterChainCallback&& fccb) override;
};

}


#pragma once
#include <drogon/HttpFilter.h>

namespace karing::filters {

class auth_filter : public drogon::HttpFilter<auth_filter> {
 public:
  void doFilter(const drogon::HttpRequestPtr& req,
                drogon::FilterCallback&& fcb,
                drogon::FilterChainCallback&& fccb) override;
};

}


#pragma once
#include <drogon/HttpFilter.h>

namespace karing::filters {

class admin_filter : public drogon::HttpFilter<admin_filter> {
 public:
  void doFilter(const drogon::HttpRequestPtr& req,
                drogon::FilterCallback&& fcb,
                drogon::FilterChainCallback&& fccb) override;
};

}


#pragma once

#include <map>
#include <optional>
#include <string>

namespace karing::cli::http {

struct response {
  long status{0};
  std::string content_type;
  std::string body;
};

response get(const std::string& base_url,
             const std::string& path,
             const std::map<std::string, std::string>& query = {},
             const std::optional<std::string>& api_key = std::nullopt);

response del(const std::string& base_url,
             const std::string& path,
             const std::map<std::string, std::string>& query = {},
             const std::optional<std::string>& api_key = std::nullopt);

response post_json(const std::string& base_url,
                   const std::string& path,
                   const std::string& json_body,
                   const std::optional<std::string>& api_key = std::nullopt);

response post(const std::string& base_url,
              const std::string& path,
              const std::map<std::string, std::string>& query = {},
              const std::optional<std::string>& api_key = std::nullopt);

response put_json(const std::string& base_url,
                  const std::string& path,
                  const std::map<std::string, std::string>& query,
                  const std::string& json_body,
                  const std::optional<std::string>& api_key = std::nullopt);

response multipart_form(const std::string& base_url,
                        const std::string& path,
                        const std::string& method,
                        const std::map<std::string, std::string>& query,
                        const std::string& file_field_name,
                        const std::string& file_path,
                        const std::optional<std::string>& filename,
                        const std::optional<std::string>& mime,
                        const std::optional<std::string>& api_key = std::nullopt);

}

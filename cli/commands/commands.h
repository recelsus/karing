#pragma once

#include <optional>
#include <string>
#include <vector>

namespace karing::cli::commands {

int run_add(const std::string& base_url,
            const std::optional<std::string>& api_key,
            bool json_output,
            const std::vector<std::string>& args);

int run_delete(const std::string& base_url,
               const std::optional<std::string>& api_key,
               bool json_output,
               const std::vector<std::string>& args);

int run_find(const std::string& base_url,
             const std::optional<std::string>& api_key,
             bool json_output,
             const std::vector<std::string>& args);

int run_get(const std::string& base_url,
            const std::optional<std::string>& api_key,
            std::optional<int> id,
            bool json_output);

int run_health(const std::string& base_url,
               const std::optional<std::string>& api_key,
               bool json_output);

int run_mod(const std::string& base_url,
            const std::optional<std::string>& api_key,
            bool json_output,
            const std::vector<std::string>& args);

}

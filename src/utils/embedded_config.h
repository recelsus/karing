#pragma once
#include <string>

namespace karing::config {

// Returns the embedded drogon.json default content.
const std::string& drogon_default_json();

// Build embedded config with an overridden port.
std::string drogon_build_config_json(int port);

// Build embedded config with TLS.
std::string drogon_build_config_json_tls(int https_port, bool https,
                                        const std::string& cert,
                                        const std::string& key,
                                        int http_port = 0,
                                        bool require_tls = false);

}

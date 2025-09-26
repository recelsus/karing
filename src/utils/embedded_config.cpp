#include "embedded_config.h"

namespace karing::config {

static std::string build_with_port(int port) {
  return std::string(R"JSON({
  "app": {
    "name": "karing",
    "threads": 0
  },
  "listeners": [
    {
      "address": "0.0.0.0",
      "port": )JSON") + std::to_string(port) + std::string(R"JSON(,
      "https": false
    }
  ],
  "log": {
    "log_level": "INFO",
    "log_path": "./logs"
  },
  "client_max_body_size": 20971520,
  "karing": {
    "limit": 100,
    "max_file_bytes": 20971520,
    "max_text_bytes": 10485760
  },
  "db_clients": []
})JSON");
}

static const std::string kEmbedded = build_with_port(8080);

const std::string& drogon_default_json() { return kEmbedded; }
std::string drogon_build_config_json(int port) { return build_with_port(port); }

std::string drogon_build_config_json_tls(int https_port, bool https,
                                        const std::string& cert,
                                        const std::string& key,
                                        int http_port,
                                        bool require_tls) {
  if (!https) return build_with_port(https_port);
  std::string listeners = "[ { \"address\": \"0.0.0.0\", \"port\": " + std::to_string(https_port) + ", \"https\": true, \"cert\": \"" + cert + "\", \"key\": \"" + key + "\" }";
  if (http_port > 0) {
    listeners += ", { \"address\": \"0.0.0.0\", \"port\": " + std::to_string(http_port) + ", \"https\": false }";
  }
  listeners += " ]";
  std::string j = std::string("{\n  \"app\": {\"name\": \"karing\", \"threads\": 0, \"require_tls\": ") + (require_tls?"true":"false") + "},\n  \"listeners\": " + listeners + ",\n  \"log\": {\"log_level\": \"INFO\", \"log_path\": \"./logs\"},\n  \"client_max_body_size\": 20971520,\n  \"karing\": {\"limit\": 100, \"max_file_bytes\": 20971520, \"max_text_bytes\": 10485760},\n  \"db_clients\": []\n}";
  return j;
}

}

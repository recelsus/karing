#include "app/bootstrap.h"

#include "app/cli_dispatcher.h"

#include <cstdlib>
#include <string>
#include <vector>
#include <filesystem>
#include <iostream>
#include <fstream>
#include <algorithm>
#include <cctype>
#include <memory>
#if defined(__linux__)
#include <unistd.h>
#endif
#include <drogon/drogon.h>

#include "version.h"
#include "db/db_init.h"
#include "db/db_introspection.h"
#include "utils/embedded_config.h"
#include "utils/options.h"
#include "utils/limits.h"

#if defined(KARING_BUILD_LIMIT) && defined(KARING_MAX_LIMIT)
#if KARING_BUILD_LIMIT > KARING_MAX_LIMIT
#error "KARING_BUILD_LIMIT must be <= KARING_MAX_LIMIT (100)"
#endif
#endif

namespace {

bool parse_json_string(const std::string& content,
                       Json::Value& out,
                       const std::string& label) {
  Json::CharReaderBuilder builder;
  builder["collectComments"] = false;
  std::string errs;
  std::unique_ptr<Json::CharReader> reader(builder.newCharReader());
  if (!reader->parse(content.data(), content.data() + content.size(), &out, &errs)) {
    LOG_ERROR << "Failed to parse " << label << ": " << errs;
    return false;
  }
  return true;
}

bool parse_json_file(const std::filesystem::path& path, Json::Value& out) {
  std::ifstream ifs(path.string());
  if (!ifs.is_open()) {
    LOG_ERROR << "Failed to open config file '" << path.string() << "'";
    return false;
  }
  Json::CharReaderBuilder builder;
  builder["collectComments"] = false;
  std::string errs;
  if (!Json::parseFromStream(builder, ifs, &out, &errs)) {
    LOG_ERROR << "Failed to parse config file '" << path.string() << "': " << errs;
    return false;
  }
  return true;
}

void merge_known(Json::Value& base,
                 const Json::Value& overrides,
                 const std::string& path = std::string()) {
  if (!base.isObject() || !overrides.isObject()) {
    LOG_WARN << "Ignoring overrides for '" << path << "' due to type mismatch";
    return;
  }
  for (const auto& key : overrides.getMemberNames()) {
    const std::string current_path = path.empty() ? key : path + "." + key;
    if (!base.isMember(key)) {
      LOG_WARN << "Ignoring unknown config key '" << current_path << "'";
      continue;
    }
    auto& target = base[key];
    const auto& incoming = overrides[key];
    if (target.isObject()) {
      if (!incoming.isObject()) {
        LOG_WARN << "Expected object at '" << current_path << "'";
        continue;
      }
      merge_known(target, incoming, current_path);
    } else if (target.isArray()) {
      if (!incoming.isArray()) {
        LOG_WARN << "Expected array at '" << current_path << "'";
        continue;
      }
      target = incoming;
    } else if (target.isBool()) {
      if (!incoming.isBool()) {
        LOG_WARN << "Expected boolean at '" << current_path << "'";
        continue;
      }
      target = incoming;
    } else if (target.isString()) {
      if (!incoming.isString()) {
        LOG_WARN << "Expected string at '" << current_path << "'";
        continue;
      }
      target = incoming;
    } else if (target.isIntegral()) {
      if (!incoming.isIntegral()) {
        LOG_WARN << "Expected integer at '" << current_path << "'";
        continue;
      }
      target = incoming;
    } else if (target.isNumeric()) {
      if (!incoming.isNumeric()) {
        LOG_WARN << "Expected number at '" << current_path << "'";
        continue;
      }
      target = incoming;
    } else {
      if (incoming.isNull()) {
        LOG_WARN << "Ignoring null override at '" << current_path << "'";
        continue;
      }
      target = incoming;
    }
  }
}

}  // namespace

namespace karing::app {

bootstrap::bootstrap(int argc_value, char** argv_value)
    : argc_(argc_value), argv_(argv_value) {}

int bootstrap::execute() {
  namespace fs = std::filesystem;
  auto& options_state = karing::options::runtime_options::instance();

  std::string config_path;
  std::string data_file;
  bool check_only = false;
  int port_override = -1;
  int limit_override = -1;
  long long file_limit_override = -1;
  long long text_limit_override = -1;
  bool tls_enable = false;
  std::string tls_cert;
  std::string tls_key;
  bool require_tls = false;
  bool no_auth_flag = false;
  bool trust_proxy_flag = false;
  bool allow_localhost_flag = false;
  std::string base_path_override;
  Json::Value drogon_config;
  if (!parse_json_string(karing::config::drogon_default_json(), drogon_config, "embedded config")) {
    return 1;
  }

  if (const char* env = std::getenv("KARING_DATA"); env && *env) data_file = env;
  if (const char* env = std::getenv("KARING_LIMIT"); env && *env) {
    try { limit_override = std::stoi(env); } catch (...) {}
  }
  if (const char* env = std::getenv("KARING_MAX_FILE_BYTES"); env && *env) {
    try { file_limit_override = std::stoll(env); } catch (...) {}
  }
  if (const char* env = std::getenv("KARING_MAX_TEXT_BYTES"); env && *env) {
    try { text_limit_override = std::stoll(env); } catch (...) {}
  }
  auto truthy = [](const char* s) {
    if (!s) return false;
    std::string v(s);
    std::transform(v.begin(), v.end(), v.begin(), [](unsigned char c) { return static_cast<char>(std::tolower(c)); });
    return v == "1" || v == "true" || v == "yes" || v == "on";
  };
  if (truthy(std::getenv("KARING_NO_AUTH"))) no_auth_flag = true;
  if (truthy(std::getenv("KARING_TRUSTED_PROXY"))) trust_proxy_flag = true;
  if (truthy(std::getenv("KARING_ALLOW_LOCALHOST"))) allow_localhost_flag = true;
  if (const char* env = std::getenv("KARING_BASE_PATH"); env && *env) base_path_override = env;

  for (int i = 1; i < argc_; ++i) {
    std::string arg = argv_[i];
    if ((arg == "--config" || arg == "-c") && i + 1 < argc_) {
      config_path = argv_[++i];
    } else if (arg == "--data" && i + 1 < argc_) {
      data_file = argv_[++i];
    } else if (arg == "--port" && i + 1 < argc_) {
      port_override = std::stoi(argv_[++i]);
    } else if (arg == "--limit" && i + 1 < argc_) {
      limit_override = std::stoi(argv_[++i]);
    } else if (arg == "--max-file-bytes" && i + 1 < argc_) {
      try { file_limit_override = std::stoll(argv_[++i]); } catch (...) {}
    } else if (arg == "--max-text-bytes" && i + 1 < argc_) {
      try { text_limit_override = std::stoll(argv_[++i]); } catch (...) {}
    } else if (arg == "--no-auth") {
      no_auth_flag = true;
    } else if (arg == "--trusted-proxy") {
      trust_proxy_flag = true;
    } else if (arg == "--allow-localhost") {
      allow_localhost_flag = true;
    } else if (arg == "--tls") {
      tls_enable = true;
    } else if (arg == "--tls-cert" && i + 1 < argc_) {
      tls_cert = argv_[++i];
    } else if (arg == "--tls-key" && i + 1 < argc_) {
      tls_key = argv_[++i];
    } else if (arg == "--require-tls") {
      require_tls = true;
    } else if (arg == "--check-db") {
      check_only = true;
    } else if ((arg == "--base-path" || arg == "--baseurl" || arg == "--base") && i + 1 < argc_) {
      base_path_override = argv_[++i];
    }
  }

  if (!config_path.empty()) {
    fs::path config_full = fs::absolute(config_path);
    Json::Value overrides;
    if (!parse_json_file(config_full, overrides)) return 1;
    merge_known(drogon_config, overrides);
  }

  if (data_file.empty()) {
    fs::path data_home;
    if (const char* xdg = std::getenv("XDG_DATA_HOME"); xdg && *xdg) data_home = fs::path(xdg);
    else if (const char* home = std::getenv("HOME"); home && *home) data_home = fs::path(home) / ".local" / "share";
    if (!data_home.empty()) data_file = (data_home / "karing" / "karing.db").string();
  }
#if defined(_WIN32)
  if (data_file.empty()) {
    if (const char* local = std::getenv("LOCALAPPDATA"); local && *local) {
      data_file = (fs::path(local) / "karing" / "karing.db").string();
    }
  }
#endif
  if (data_file.empty()) {
    fs::path exec_path;
#if defined(__linux__)
    char buf[4096];
    ssize_t len = ::readlink("/proc/self/exe", buf, sizeof(buf) - 1);
    if (len > 0) {
      buf[len] = '\0';
      exec_path = fs::path(buf);
    } else {
      LOG_WARN << "readlink(/proc/self/exe) failed; falling back to argv[0]";
    }
#endif
    if (exec_path.empty() && argc_ > 0) exec_path = fs::absolute(argv_[0]);
    auto base_dir = exec_path.empty() ? fs::current_path() : exec_path.parent_path();
    data_file = (base_dir / "karing.db").string();
  }

  fs::path data_path = fs::absolute(data_file);
  {
    std::error_code ec;
    fs::create_directories(data_path.parent_path(), ec);
    if (ec) {
      LOG_ERROR << "Failed to create data directory '" << data_path.parent_path().string() << "': " << ec.message();
      return 1;
    }
    fs::current_path(data_path.parent_path(), ec);
    if (ec) {
      LOG_ERROR << "Failed to change working directory to '" << data_path.parent_path().string() << "': " << ec.message();
      return 1;
    }
  }

  const auto resolved_db = karing::db::ensure_db_path(data_path.string());

  try {
    const auto cfg_name = std::string("karing.db");
    if (data_path.filename() != cfg_name) {
      std::error_code ec;
      if (!fs::exists(cfg_name)) {
        fs::create_symlink(data_path.filename(), cfg_name, ec);
        if (ec) {
          LOG_WARN << "Failed to create symlink '" << cfg_name << "' -> '" << data_path.filename().string() << "': " << ec.message();
        } else {
          LOG_INFO << "Created symlink '" << cfg_name << "' -> '" << data_path.filename().string() << "'";
        }
      }
    }
  } catch (...) {}

  karing::db::init_sqlite_schema_file(resolved_db);

  {
    cli_dispatcher dispatcher(argc_, argv_, resolved_db);
    if (auto cli_exit = dispatcher.run(); cli_exit.has_value()) return *cli_exit;
  }

  if (tls_enable || port_override > 0) {
    if (tls_enable) {
      int https_port = port_override > 0 ? port_override : 8080;
      int http_port = (port_override > 0 ? 0 : 8080);
      Json::Value listeners(Json::arrayValue);
      Json::Value https_listener(Json::objectValue);
      https_listener["address"] = "0.0.0.0";
      https_listener["port"] = https_port;
      https_listener["https"] = true;
      https_listener["cert"] = tls_cert.empty() ? "server.crt" : tls_cert;
      https_listener["key"] = tls_key.empty() ? "server.key" : tls_key;
      listeners.append(https_listener);
      if (http_port > 0) {
        Json::Value http_listener(Json::objectValue);
        http_listener["address"] = "0.0.0.0";
        http_listener["port"] = http_port;
        http_listener["https"] = false;
        listeners.append(http_listener);
      }
      drogon_config["listeners"] = listeners;
      if (drogon_config.isMember("app") && drogon_config["app"].isObject()) {
        drogon_config["app"]["require_tls"] = require_tls;
      }
    } else {
      int port = port_override > 0 ? port_override : 8080;
      Json::Value listeners(Json::arrayValue);
      Json::Value listener(Json::objectValue);
      listener["address"] = "0.0.0.0";
      listener["port"] = port;
      listener["https"] = false;
      listeners.append(listener);
      drogon_config["listeners"] = listeners;
      if (drogon_config.isMember("app") && drogon_config["app"].isObject()) {
        drogon_config["app"]["require_tls"] = false;
      }
    }
  }

  try { fs::create_directories("logs"); } catch (...) {}
  drogon::app().loadConfigJson(drogon_config);
  if (const char* env_log = std::getenv("KARING_LOG_PATH"); env_log && *env_log) {
    try { fs::create_directories(env_log); } catch (...) {}
    drogon::app().setLogPath(env_log);
  }
  try {
    std::string configured_log;
    if (drogon_config.isMember("log") &&
        drogon_config["log"].isMember("log_path") &&
        drogon_config["log"]["log_path"].isString()) {
      configured_log = drogon_config["log"]["log_path"].asString();
    }
    bool need_override = configured_log.empty();
#if !defined(_WIN32)
    if (!need_override && !configured_log.empty() && configured_log[0] != '/') need_override = true;
#endif
    if (need_override) {
      fs::path state_home;
#if defined(_WIN32)
      if (const char* local = std::getenv("LOCALAPPDATA"); local && *local) state_home = fs::path(local) / "karing" / "logs";
#else
      if (const char* xdg = std::getenv("XDG_STATE_HOME"); xdg && *xdg) state_home = fs::path(xdg) / "karing" / "logs";
      else if (const char* home = std::getenv("HOME"); home && *home) state_home = fs::path(home) / ".local" / "state" / "karing" / "logs";
#endif
      if (!state_home.empty()) {
        std::error_code ec;
        fs::create_directories(state_home, ec);
        drogon::app().setLogPath(state_home.string());
      }
    }
  } catch (...) {}

  {
    bool require_tls_config = false;
    if (drogon_config.isMember("app") &&
        drogon_config["app"].isMember("require_tls") &&
        drogon_config["app"]["require_tls"].isBool()) {
      require_tls_config = drogon_config["app"]["require_tls"].asBool();
    }
    int https_port = 0;
    int http_port = 0;
    std::string cert_path;
    std::string key_path;
    if (drogon_config.isMember("listeners") && drogon_config["listeners"].isArray()) {
      for (const auto& listener : drogon_config["listeners"]) {
        if (!listener.isObject()) continue;
        bool is_https = listener.isMember("https") && listener["https"].asBool();
        int port = 0;
        if (listener.isMember("port")) {
          if (listener["port"].isInt()) port = listener["port"].asInt();
          else if (listener["port"].isUInt()) port = static_cast<int>(listener["port"].asUInt());
        }
        if (is_https) {
          if (https_port == 0 && port > 0) https_port = port;
          if (listener.isMember("cert") && listener["cert"].isString()) cert_path = listener["cert"].asString();
          if (listener.isMember("key") && listener["key"].isString()) key_path = listener["key"].asString();
        } else {
          if (http_port == 0 && port > 0) http_port = port;
        }
      }
    }
    options_state.set_tls(https_port > 0, require_tls_config, https_port, http_port);
    options_state.set_tls_cert_paths(cert_path, key_path);
  }

  try {
    auto cfg = drogon::app().getCustomConfig();
    if (cfg.isMember("karing") && cfg["karing"].isMember("base_path") && cfg["karing"]["base_path"].isString()) {
      options_state.set_base_path(cfg["karing"]["base_path"].asString());
    }
    std::vector<std::string> proxies;
    auto collect = [&](const Json::Value& v) {
      if (v.isArray()) {
        for (const auto& e : v) if (e.isString()) proxies.emplace_back(e.asString());
      } else if (v.isString()) {
        std::string s = v.asString();
        size_t pos = 0;
        while (pos <= s.size()) {
          size_t q = s.find(',', pos);
          if (q == std::string::npos) q = s.size();
          std::string t = s.substr(pos, q - pos);
          pos = q + 1;
          while (!t.empty() && t.front() == ' ') t.erase(t.begin());
          while (!t.empty() && t.back() == ' ') t.pop_back();
          if (!t.empty()) proxies.push_back(t);
        }
      }
    };
    if (cfg.isMember("app") && cfg["app"].isMember("trusted_proxies")) collect(cfg["app"]["trusted_proxies"]);
    if (cfg.isMember("karing") && cfg["karing"].isMember("trusted_proxies")) collect(cfg["karing"]["trusted_proxies"]);
    if (!proxies.empty()) options_state.set_trusted_proxies(proxies);
  } catch (...) {}

  if (!base_path_override.empty()) options_state.set_base_path(base_path_override);

  int limit_value = 100;
  try {
    auto cfg = drogon::app().getCustomConfig();
    if (cfg.isMember("karing") && cfg["karing"].isMember("limit") && cfg["karing"]["limit"].isInt()) {
      limit_value = cfg["karing"]["limit"].asInt();
    }
  } catch (...) {}
  if (limit_value == 100) {
    try {
      if (drogon_config.isMember("karing") &&
          drogon_config["karing"].isMember("limit") &&
          drogon_config["karing"]["limit"].isInt()) {
        limit_value = drogon_config["karing"]["limit"].asInt();
      }
    } catch (...) {}
  }
  if (limit_override > 0) limit_value = limit_override;
  if (limit_value > KARING_BUILD_LIMIT) {
    LOG_WARN << "limit " << limit_value << " exceeds build limit " << KARING_BUILD_LIMIT << "; clamping.";
    limit_value = KARING_BUILD_LIMIT;
  }
  if (limit_value > KARING_MAX_LIMIT) limit_value = KARING_MAX_LIMIT;
  if (limit_value < 1) limit_value = 1;

  karing::db::init_sqlite_schema_file(resolved_db);

  const long long hard_file_max = 20971520LL;
  long long max_file_bytes = hard_file_max;
  try {
    auto cfg = drogon::app().getCustomConfig();
    if (cfg.isMember("karing") && cfg["karing"].isMember("max_file_bytes") && cfg["karing"]["max_file_bytes"].isInt64()) {
      max_file_bytes = cfg["karing"]["max_file_bytes"].asInt64();
    }
  } catch (...) {}
  if (file_limit_override > 0) max_file_bytes = file_limit_override;
  if (max_file_bytes > hard_file_max) {
    LOG_WARN << "max_file_bytes " << max_file_bytes << " exceeds hard cap " << hard_file_max << "; clamping.";
    max_file_bytes = hard_file_max;
  }
  if (max_file_bytes < 1) max_file_bytes = 1;

  const long long hard_text_max = 10485760LL;
  long long max_text_bytes = hard_text_max;
  try {
    auto cfg = drogon::app().getCustomConfig();
    if (cfg.isMember("karing") && cfg["karing"].isMember("max_text_bytes") && cfg["karing"]["max_text_bytes"].isInt64()) {
      max_text_bytes = cfg["karing"]["max_text_bytes"].asInt64();
    }
  } catch (...) {}
  if (text_limit_override > 0) max_text_bytes = text_limit_override;
  if (max_text_bytes > hard_text_max) {
    LOG_WARN << "max_text_bytes " << max_text_bytes << " exceeds hard cap " << hard_text_max << "; clamping.";
    max_text_bytes = hard_text_max;
  }
  if (max_text_bytes < 1) max_text_bytes = 1;

  options_state.set_runtime(resolved_db,
                            KARING_BUILD_LIMIT,
                            limit_value,
                            static_cast<int>(max_file_bytes),
                            static_cast<int>(max_text_bytes),
                            no_auth_flag,
                            trust_proxy_flag,
                            allow_localhost_flag);

  int shown_port = 0;
  try {
    auto cfg = drogon::app().getCustomConfig();
    if (cfg.isMember("listeners") && cfg["listeners"].isArray() && !cfg["listeners"].empty()) {
      const auto& l0 = cfg["listeners"][0U];
      if (l0.isMember("port") && l0["port"].isInt()) shown_port = l0["port"].asInt();
    }
  } catch (...) {}
  if (shown_port == 0) {
    try {
      if (drogon_config.isMember("listeners") &&
          drogon_config["listeners"].isArray() &&
          !drogon_config["listeners"].empty()) {
        const auto& l0 = drogon_config["listeners"][0U];
        if (l0.isMember("port") && l0["port"].isInt()) shown_port = l0["port"].asInt();
      }
    } catch (...) {}
  }

  LOG_INFO << "karing " << KARING_VERSION
           << " | port " << shown_port
           << " | db " << data_path.string()
           << " | limit " << limit_value << "/" << KARING_BUILD_LIMIT
           << " | max_file_bytes " << max_file_bytes << "/20971520"
           << " | max_text_bytes " << max_text_bytes << "/10485760"
           << " | no_auth " << (no_auth_flag ? 1 : 0)
           << " | allow_localhost " << (allow_localhost_flag ? 1 : 0)
           << " | trusted_proxy " << (trust_proxy_flag ? 1 : 0);

  if (check_only) {
    auto tables = karing::db::inspect::list_tables_with_sql(resolved_db);
    std::cout << "Tables (" << tables.size() << ")\n";
    for (const auto& t : tables) std::cout << "- " << t.first << "\n";
    std::cout << "limit=" << limit_value << " (build-limit=" << KARING_BUILD_LIMIT << ")\n";
    std::cout << "max_file_bytes=" << max_file_bytes << " (hard-cap=20971520)\n";
    std::cout << "max_text_bytes=" << max_text_bytes << " (hard-cap=10485760)\n";
    return 0;
  }

  {
    const auto base = options_state.base_path();
    if (!base.empty() && base != "/") {
      std::string p = base;
      if (p.size() > 1 && p.back() == '/') p.pop_back();
      drogon::app().registerPreRoutingAdvice([p](const drogon::HttpRequestPtr& req,
                                                 drogon::AdviceCallback&& cb,
                                                 drogon::AdviceChainCallback&& next) {
        const auto& path = req->path();
        if (!p.empty() && (path == p || (path.rfind(p, 0) == 0 && (path.size() == p.size() || path[p.size()] == '/')))) {
          std::string np = path.substr(p.size());
          if (np.empty()) np = "/";
          req->setPath(std::move(np));
        }
        next();
      });
    }
  }

  drogon::app().run();
  return 0;
}

}  // namespace karing::app

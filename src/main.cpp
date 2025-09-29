// Minimal bootstrap for karing using Drogon
#include <cstdlib>
#include <string>
#include <vector>
#include <filesystem>
#include <iostream>
#include <fstream>
#include <sstream>
#include <algorithm>
#include <cctype>
#include <sqlite3.h>
#include <openssl/rand.h>
#if defined(__linux__)
#include <unistd.h>
#endif
#include <drogon/drogon.h>
#include "version.h"
#include "db/db_init.h"
#include "db/db_introspection.h"
#include "utils/embedded_config.h"
#include "dao/karing_dao.h"
#include "utils/options.h"
#include "utils/limits.h"
#include "controllers/karing_controller.h"
#include "controllers/health_controller.h"

// Build-time safety: ensure configured build limit <= absolute maximum
#if defined(KARING_BUILD_LIMIT) && defined(KARING_MAX_LIMIT)
#if KARING_BUILD_LIMIT > KARING_MAX_LIMIT
#error "KARING_BUILD_LIMIT must be <= KARING_MAX_LIMIT (100)"
#endif
#endif

int main(int argc, char* argv[]) {
  namespace fs = std::filesystem;

  // Parse very small set of args: --config, --data, --port, --limit, --max-file-bytes, --max-text-bytes, --no-auth, --trusted-proxy, --allow-localhost, --check-db
  std::string configPath; // resolved later with XDG and fallback to config/karing.json
  std::string dataFile; // SQLite DB file path
  bool checkOnly = false;
  int portOverride = -1;
  int limitOverride = -1; // from CLI/ENV
  long long fileLimitOverride = -1; // from CLI/ENV
  long long textLimitOverride = -1; // from CLI/ENV
  // TLS flags
  bool tlsEnable = false;
  std::string tlsCert, tlsKey;
  bool requireTls = false;
  bool noAuthFlag = false;
  bool trustProxyFlag = false;
  bool allowLocalhostFlag = false;
  std::string basePathOverride; // CLI/ENV base path
  if (const char* env = std::getenv("KARING_CONFIG"); env && *env) configPath = env;
  if (const char* env = std::getenv("KARING_DATA"); env && *env) dataFile = env;
  // Read environment first so that CLI args can override them (args have higher precedence)
  if (const char* env = std::getenv("KARING_LIMIT"); env && *env) {
    try { limitOverride = std::stoi(env); } catch (...) {}
  }
  if (const char* env = std::getenv("KARING_MAX_FILE_BYTES"); env && *env) {
    try { fileLimitOverride = std::stoll(env); } catch (...) {}
  }
  if (const char* env = std::getenv("KARING_MAX_TEXT_BYTES"); env && *env) {
    try { textLimitOverride = std::stoll(env); } catch (...) {}
  }
  auto truthy = [](const char* s){ if (!s) return false; std::string v(s); std::transform(v.begin(), v.end(), v.begin(), [](unsigned char c){ return (char)std::tolower(c); });
                                   return v=="1" || v=="true" || v=="yes" || v=="on"; };
  if (truthy(std::getenv("KARING_NO_AUTH"))) noAuthFlag = true;
  if (truthy(std::getenv("KARING_TRUSTED_PROXY"))) trustProxyFlag = true;
  if (truthy(std::getenv("KARING_ALLOW_LOCALHOST"))) allowLocalhostFlag = true;
  if (const char* env = std::getenv("KARING_BASE_PATH"); env && *env) basePathOverride = env;

  for (int i = 1; i < argc; ++i) {
    std::string arg = argv[i];
    if ((arg == "--config" || arg == "-c") && i + 1 < argc) {
      configPath = argv[++i];
    } else if (arg == "--data" && i + 1 < argc) {
      dataFile = argv[++i];
    } else if (arg == "--port" && i + 1 < argc) {
      portOverride = std::stoi(argv[++i]);
    } else if (arg == "--limit" && i + 1 < argc) {
      limitOverride = std::stoi(argv[++i]);
    } else if (arg == "--max-file-bytes" && i + 1 < argc) {
      try { fileLimitOverride = std::stoll(argv[++i]); } catch (...) {}
    } else if (arg == "--max-text-bytes" && i + 1 < argc) {
      try { textLimitOverride = std::stoll(argv[++i]); } catch (...) {}
    } else if (arg == "--no-auth") {
      noAuthFlag = true;
    } else if (arg == "--trusted-proxy") {
      trustProxyFlag = true;
    } else if (arg == "--allow-localhost") {
      allowLocalhostFlag = true;
    } else if (arg == "--tls") {
      tlsEnable = true;
    } else if (arg == "--tls-cert" && i + 1 < argc) {
      tlsCert = argv[++i];
    } else if (arg == "--tls-key" && i + 1 < argc) {
      tlsKey = argv[++i];
    } else if (arg == "--require-tls") {
      requireTls = true;
    } else if (arg == "--check-db") {
      checkOnly = true;
    } else if ((arg == "--base-path" || arg == "--baseurl" || arg == "--base") && i + 1 < argc) {
      basePathOverride = argv[++i];
    }
  }
  // env already read above; CLI overrides applied in the loop

  // Resolve config path with precedence: --config/env > XDG_CONFIG_HOME > ~/.config > bundled config
  bool usingDefaultBundledConfig = false;
  fs::path configAbs;
  if (!configPath.empty()) {
    configAbs = fs::absolute(configPath);
  } else {
    // XDG_CONFIG_HOME/karing/karing.json or ~/.config/karing/karing.json
    fs::path cfgHome;
    if (const char* xdg = std::getenv("XDG_CONFIG_HOME"); xdg && *xdg) cfgHome = fs::path(xdg);
    else if (const char* home = std::getenv("HOME"); home && *home) cfgHome = fs::path(home) / ".config";
    if (!cfgHome.empty()) {
      fs::path xdgCfg = cfgHome / "karing" / "karing.json";
      if (fs::exists(xdgCfg)) configAbs = fs::absolute(xdgCfg);
    }
    if (configAbs.empty()) {
#if defined(_WIN32)
      if (const char* appdata = std::getenv("APPDATA"); appdata && *appdata) {
        fs::path winCfg = fs::path(appdata) / "karing" / "karing.json";
        if (fs::exists(winCfg)) configAbs = fs::absolute(winCfg);
      }
#endif
    }
    if (configAbs.empty()) {
      // System-wide config (POSIX): /etc/karing/karing.json
#if !defined(_WIN32)
      fs::path etcCfg = fs::path("/etc") / "karing" / "karing.json";
      if (fs::exists(etcCfg)) configAbs = fs::absolute(etcCfg);
#endif
    }
    if (configAbs.empty()) {
      fs::path bundled = fs::absolute("config/karing.json");
      if (fs::exists(bundled)) { configAbs = bundled; usingDefaultBundledConfig = true; }
    }
  }
  // If still empty, we'll generate an embedded config later

  // Determine default DB file path. Precedence: --data/env > XDG_DATA_HOME > ~/.local/share > executable directory
  if (dataFile.empty()) {
    // Prefer XDG data home
    fs::path dataHome;
    if (const char* xdg = std::getenv("XDG_DATA_HOME"); xdg && *xdg) dataHome = fs::path(xdg);
    else if (const char* home = std::getenv("HOME"); home && *home) dataHome = fs::path(home) / ".local" / "share";
    if (!dataHome.empty()) {
      auto p = dataHome / "karing" / "karing.db";
      dataFile = p.string();
    }
  }
  // Windows fallback: %LOCALAPPDATA%\karing\karing.db
#if defined(_WIN32)
  if (dataFile.empty()) {
    if (const char* local = std::getenv("LOCALAPPDATA"); local && *local) {
      dataFile = (fs::path(local) / "karing" / "karing.db").string();
    }
  }
#endif
  if (dataFile.empty()) {
    fs::path execPath;
#if defined(__linux__)
    // Linux: /proc/self/exe
    char buf[4096];
    ssize_t len = ::readlink("/proc/self/exe", buf, sizeof(buf) - 1);
    if (len > 0) { buf[len] = '\0'; execPath = fs::path(buf); }
#endif
    if (execPath.empty() && argc > 0) {
      execPath = fs::absolute(argv[0]);
    }
    auto baseDir = execPath.empty() ? fs::current_path() : execPath.parent_path();
    dataFile = (baseDir / "karing.db").string();
  }

  // Create and change to the DB's parent directory so config relative paths (db, logs) resolve there
  fs::path dataPath = fs::absolute(dataFile);
  fs::create_directories(dataPath.parent_path());
  fs::current_path(dataPath.parent_path());

  // Ensure DB file exists at the chosen path (no SQLite calls yet)
  const auto resolvedDb = karing::db::ensure_db_path(dataPath.string());
  LOG_DEBUG << "SQLite path prepared at: " << resolvedDb;

  // If the configured DB filename in the config is different (./karing.db), create a symlink
  // so Drogon opens the intended file without rewriting the config.
  try {
    const auto cfgDbName = std::string("karing.db");
    if (dataPath.filename() != cfgDbName) {
      std::error_code ec;
      if (!fs::exists(cfgDbName)) {
        fs::create_symlink(dataPath.filename(), cfgDbName, ec);
        if (ec) {
          LOG_WARN << "Failed to create symlink '" << cfgDbName << "' -> '" << dataPath.filename().string() << "': " << ec.message();
        } else {
          LOG_INFO << "Created symlink '" << cfgDbName << "' -> '" << dataPath.filename().string() << "'";
        }
      }
    }
  } catch (...) {
    // ignore symlink errors
  }

  // Choose configuration source: explicit path or embedded default (with optional port override)
  // If bundled config/karing.json exists, use it as default (do NOT override with embedded).
  if (configAbs.empty()) {
    // No explicit override given; use embedded config. Persist to a local file for Drogon.
    std::string jsonContent;
    if (portOverride > 0 || tlsEnable) {
      int p = portOverride > 0 ? portOverride : 8080;
      if (tlsEnable) {
        int httpPort = (portOverride>0?0:8080);
        jsonContent = karing::config::drogon_build_config_json_tls(p, true, tlsCert.empty()?"server.crt":tlsCert, tlsKey.empty()?"server.key":tlsKey, httpPort, requireTls);
        karing::options::set_tls(true, requireTls, p, httpPort);
        karing::options::set_tls_cert_paths(tlsCert, tlsKey);
      } else {
        jsonContent = karing::config::drogon_build_config_json(p);
        karing::options::set_tls(false, false, 0, p);
      }
    } else {
      jsonContent = karing::config::drogon_default_json();
      karing::options::set_tls(false, false, 0, 8080);
    }
    // Write embedded config for Drogon to load from the working directory
    const std::string embeddedPath = (fs::current_path() / "drogon.embedded.json").string();
    {
      std::ofstream ofs(embeddedPath);
      ofs << jsonContent;
    }
    configAbs = fs::absolute(embeddedPath);

    // First-run experience: also materialize a user config file if none exists in XDG/OS-default location
    fs::path userCfg;
#if defined(_WIN32)
    if (const char* appdata = std::getenv("APPDATA"); appdata && *appdata) {
      userCfg = fs::path(appdata) / "karing" / "karing.json";
    }
#else
    if (const char* xdg = std::getenv("XDG_CONFIG_HOME"); xdg && *xdg) {
      userCfg = fs::path(xdg) / "karing" / "karing.json";
    } else if (const char* home = std::getenv("HOME"); home && *home) {
      userCfg = fs::path(home) / ".config" / "karing" / "karing.json";
    }
#endif
    if (!userCfg.empty()) {
      std::error_code ec;
      fs::create_directories(userCfg.parent_path(), ec);
      if (!fs::exists(userCfg)) {
        std::ofstream ucfg(userCfg.string());
        ucfg << jsonContent;
      }
    }
  }
  // Load Drogon configuration
  drogon::app().loadConfigFile(configAbs.string());
  // KARING_LOG_PATH env overrides log path if provided; else XDG state if config is missing/relative
  if (const char* envLog = std::getenv("KARING_LOG_PATH"); envLog && *envLog) {
    try { std::filesystem::create_directories(envLog); } catch (...) {}
    drogon::app().setLogPath(envLog);
  }
  // Override log path to XDG state if config uses a relative or missing path
  try {
    std::string configured_log;
    {
      std::ifstream ifs(configAbs.string());
      if (ifs) {
        Json::CharReaderBuilder b; Json::Value root; std::string errs;
        if (Json::parseFromStream(b, ifs, &root, &errs)) {
          if (root.isMember("log") && root["log"].isMember("log_path") && root["log"]["log_path"].isString()) {
            configured_log = root["log"]["log_path"].asString();
          }
        }
      }
    }
    bool need_override = configured_log.empty();
#if !defined(_WIN32)
    if (!need_override && !configured_log.empty() && configured_log[0] != '/') need_override = true;
#endif
    if (need_override) {
      std::filesystem::path state_home;
#if defined(_WIN32)
      if (const char* local = std::getenv("LOCALAPPDATA"); local && *local) state_home = std::filesystem::path(local) / "karing" / "logs";
#else
      if (const char* xdg = std::getenv("XDG_STATE_HOME"); xdg && *xdg) state_home = std::filesystem::path(xdg) / "karing" / "logs";
      else if (const char* home = std::getenv("HOME"); home && *home) state_home = std::filesystem::path(home) / ".local" / "state" / "karing" / "logs";
#endif
      if (!state_home.empty()) {
        std::error_code ec; std::filesystem::create_directories(state_home, ec);
        drogon::app().setLogPath(state_home.string());
      }
    }
  } catch (...) {}
  // Ensure per-user config materialised for future runs if absent, using the selected config
  {
#if defined(_WIN32)
    fs::path userCfg;
    if (const char* appdata = std::getenv("APPDATA"); appdata && *appdata) {
      userCfg = fs::path(appdata) / "karing" / "karing.json";
    }
#else
    fs::path userCfg;
    if (const char* xdg = std::getenv("XDG_CONFIG_HOME"); xdg && *xdg) {
      userCfg = fs::path(xdg) / "karing" / "karing.json";
    } else if (const char* home = std::getenv("HOME"); home && *home) {
      userCfg = fs::path(home) / ".config" / "karing" / "karing.json";
    }
#endif
    if (!userCfg.empty()) {
      std::error_code ec;
      fs::create_directories(userCfg.parent_path(), ec);
      if (!fs::exists(userCfg)) {
        try {
          fs::copy_file(configAbs, userCfg, fs::copy_options::none, ec);
        } catch (...) {}
      }
    }
  }
  // External config integration: base_path, app.require_tls, trusted_proxies
  try {
    auto cfg = drogon::app().getCustomConfig();
    if (cfg.isMember("karing") && cfg["karing"].isMember("base_path") && cfg["karing"]["base_path"].isString()) {
      karing::options::set_base_path(cfg["karing"]["base_path"].asString());
    }
    if (cfg.isMember("app") && cfg["app"].isMember("require_tls") && cfg["app"]["require_tls"].isBool()) {
      karing::options::set_tls(karing::options::tls_enabled(), cfg["app"]["require_tls"].asBool(), karing::options::tls_https_port(), karing::options::tls_http_port());
    }
    // trusted proxies from config: app.trusted_proxies as array or karing.trusted_proxies as array/CSV
    std::vector<std::string> proxies;
    auto collect = [&](const Json::Value& v){ if (v.isArray()) { for (const auto& e: v) if (e.isString()) proxies.emplace_back(e.asString()); } else if (v.isString()) { std::string s=v.asString(); size_t pos=0; while (pos<=s.size()) { size_t q=s.find(',',pos); if (q==std::string::npos) q=s.size(); std::string t=s.substr(pos,q-pos); pos=q+1; while(!t.empty()&&t.front()==' ') t.erase(t.begin()); while(!t.empty()&&t.back()==' ') t.pop_back(); if(!t.empty()) proxies.push_back(t);} } };
    if (cfg.isMember("app") && cfg["app"].isMember("trusted_proxies")) collect(cfg["app"]["trusted_proxies"]);
    if (cfg.isMember("karing") && cfg["karing"].isMember("trusted_proxies")) collect(cfg["karing"]["trusted_proxies"]);
    if (!proxies.empty()) karing::options::set_trusted_proxies(proxies);
  } catch (...) {}

  // Override base_path from CLI/ENV if provided
  if (!basePathOverride.empty()) {
    karing::options::set_base_path(basePathOverride);
  }

  // Determine effective limit: config -> override, clamped to build limit and 100
  int limitValue = 100; // default
  try {
    auto cfg = drogon::app().getCustomConfig();
    if (cfg.isMember("karing") && cfg["karing"].isMember("limit") && cfg["karing"]["limit"].isInt()) {
      limitValue = cfg["karing"]["limit"].asInt();
    }
  } catch (...) {}
  // Fallback: robust parse from the loaded config file if customConfig did not expose the value
  if (limitValue == 100) {
    try {
      std::ifstream ifs(configAbs.string());
      if (ifs) {
        Json::CharReaderBuilder b; Json::Value root; std::string errs;
        if (Json::parseFromStream(b, ifs, &root, &errs)) {
          if (root.isMember("karing") && root["karing"].isMember("limit") && root["karing"]["limit"].isInt()) {
            limitValue = root["karing"]["limit"].asInt();
          }
        }
      }
    } catch (...) {}
  }
  if (limitOverride > 0) limitValue = limitOverride;
  if (limitValue > KARING_BUILD_LIMIT) {
    LOG_WARN << "limit " << limitValue << " exceeds build limit " << KARING_BUILD_LIMIT << "; clamping.";
    limitValue = KARING_BUILD_LIMIT;
  }
  if (limitValue > KARING_MAX_LIMIT) limitValue = KARING_MAX_LIMIT;
  if (limitValue < 1) limitValue = 1;

  // Initialize schema directly via SQLite C API to avoid Drogon DB config conflicts
  karing::db::init_sqlite_schema_file(resolvedDb);

  // Slot preallocation removed; overwrite-on-limit policy is used.

  // Determine effective max file size (bytes): config -> override, clamp to 20 MiB
  const long long HARD_FILE_MAX = 20971520LL; // 20 MiB
  long long maxFileBytes = HARD_FILE_MAX; // default
  try {
    auto cfg = drogon::app().getCustomConfig();
    if (cfg.isMember("karing") && cfg["karing"].isMember("max_file_bytes") && cfg["karing"]["max_file_bytes"].isInt64()) {
      maxFileBytes = cfg["karing"]["max_file_bytes"].asInt64();
    }
  } catch (...) {}
  if (fileLimitOverride > 0) maxFileBytes = fileLimitOverride;
  if (maxFileBytes > HARD_FILE_MAX) {
    LOG_WARN << "max_file_bytes " << maxFileBytes << " exceeds hard cap " << HARD_FILE_MAX << "; clamping.";
    maxFileBytes = HARD_FILE_MAX;
  }
  if (maxFileBytes < 1) maxFileBytes = 1;

  // Determine effective max text size (bytes): config -> override, clamp to 10 MiB
  const long long HARD_TEXT_MAX = 10485760LL; // 10 MiB
  long long maxTextBytes = HARD_TEXT_MAX; // default
  try {
    auto cfg = drogon::app().getCustomConfig();
    if (cfg.isMember("karing") && cfg["karing"].isMember("max_text_bytes") && cfg["karing"]["max_text_bytes"].isInt64()) {
      maxTextBytes = cfg["karing"]["max_text_bytes"].asInt64();
    }
  } catch (...) {}
  if (textLimitOverride > 0) maxTextBytes = textLimitOverride;
  if (maxTextBytes > HARD_TEXT_MAX) {
    LOG_WARN << "max_text_bytes " << maxTextBytes << " exceeds hard cap " << HARD_TEXT_MAX << "; clamping.";
    maxTextBytes = HARD_TEXT_MAX;
  }
  if (maxTextBytes < 1) maxTextBytes = 1;

  // Publish runtime options
  karing::options::set_runtime(resolvedDb, KARING_BUILD_LIMIT, limitValue, (int)maxFileBytes, (int)maxTextBytes, noAuthFlag, trustProxyFlag, allowLocalhostFlag);
  int shownPort = 0;
  try {
    auto cfg = drogon::app().getCustomConfig();
    if (cfg.isMember("listeners") && cfg["listeners"].isArray() && !cfg["listeners"].empty()) {
      const auto& l0 = cfg["listeners"][0U];
      if (l0.isMember("port") && l0["port"].isInt()) shownPort = l0["port"].asInt();
    }
  } catch (...) {}
  if (shownPort == 0) {
    // Fallback: parse loaded config file directly
    try {
      std::ifstream ifs(configAbs.string());
      if (ifs) {
        Json::CharReaderBuilder b; Json::Value root; std::string errs;
        if (Json::parseFromStream(b, ifs, &root, &errs)) {
          if (root.isMember("listeners") && root["listeners"].isArray() && !root["listeners"].empty()) {
            const auto& l0 = root["listeners"][0U];
            if (l0.isMember("port") && l0["port"].isInt()) shownPort = l0["port"].asInt();
          }
        }
      }
    } catch (...) {}
  }

  LOG_INFO << "karing " << KARING_VERSION
           << " | port " << shownPort
           << " | db " << dataPath.string()
           << " | limit " << limitValue << "/" << KARING_BUILD_LIMIT
           << " | max_file_bytes " << maxFileBytes << "/20971520"
           << " | max_text_bytes " << maxTextBytes << "/10485760"
           << " | no_auth " << (noAuthFlag?1:0)
           << " | allow_localhost " << (allowLocalhostFlag?1:0)
           << " | trusted_proxy " << (trustProxyFlag?1:0);
  // If only checking DB, print tables then exit
  if (checkOnly) {
    auto tables = karing::db::inspect::list_tables_with_sql(resolvedDb);
    std::cout << "Tables (" << tables.size() << ")\n";
    for (const auto& t : tables) {
      std::cout << "- " << t.first << "\n";
    }
    std::cout << "limit=" << limitValue << " (build-limit=" << KARING_BUILD_LIMIT << ")\n";
    std::cout << "max_file_bytes=" << maxFileBytes << " (hard-cap=20971520)\n";
    std::cout << "max_text_bytes=" << maxTextBytes << " (hard-cap=10485760)\n";
    return 0;
  }

  // API key / IP ACL operations (CLI)
  // New subcommand interface: `karing keys ...` and `karing ip ...`
  // Legacy flag interface still supported for backward compatibility.
  auto hasArg = [&](const char* flag){ return std::find_if(argv+1, argv+argc, [&](const char* a){return std::string(a)==flag;}) != argv+argc; };
  auto argValue = [&](const char* flag)->std::optional<std::string>{ for (int i=1;i<argc-1;++i) if (std::string(argv[i])==flag) return std::string(argv[i+1]); return std::nullopt; };

  auto generate_secret_hex = []() -> std::optional<std::string> {
    // Generate 192-bit secret as hex (32 chars)
    unsigned char buf[24];
    if (RAND_bytes(buf, sizeof(buf)) != 1) return std::nullopt;
    static const char* hex = "0123456789abcdef";
    std::string key; key.resize(sizeof(buf) * 2);
    for (size_t i=0;i<sizeof(buf);++i) { key[i*2] = hex[(buf[i]>>4)&0xF]; key[i*2+1] = hex[buf[i]&0xF]; }
    return key;
  };

  auto normalize_ipv4_cidr = [](const std::string& s) -> std::string {
    // Accepts forms: a.b.c.d, a.b.c.d/len; returns normalised CIDR if prefix present
    // No validation for IPv6 here; returns input unchanged if not IPv4
    auto slash = s.find('/');
    std::string ip = s.substr(0, slash);
    int bits = -1;
    if (slash != std::string::npos) {
      try { bits = std::stoi(s.substr(slash+1)); } catch (...) { bits = -1; }
      if (bits < 0 || bits > 32) return s; // leave as-is
    }
    int o[4]={0,0,0,0}; char c;
    std::istringstream iss(ip);
    if (!(iss >> o[0] >> c >> o[1] >> c >> o[2] >> c >> o[3])) return s;
    for (int i=0;i<4;++i) { if (o[i] < 0 || o[i] > 255) return s; }
    if (bits < 0) return s; // no prefix -> leave unchanged
    uint32_t v = (uint32_t)((o[0]<<24) | (o[1]<<16) | (o[2]<<8) | o[3]);
    uint32_t mask = (bits==0)? 0u : 0xFFFFFFFFu << (32 - bits);
    uint32_t net = v & mask;
    int n0 = (net>>24)&0xFF, n1 = (net>>16)&0xFF, n2 = (net>>8)&0xFF, n3 = net & 0xFF;
    std::ostringstream oss; oss << n0 << "." << n1 << "." << n2 << "." << n3 << "/" << bits;
    return oss.str();
  };

  // Subcommand dispatcher
  if (argc >= 2) {
    std::string cmd1 = argv[1];
    if (cmd1 == "keys") {
      if (argc >= 3) {
        std::string cmd2 = argv[2];
        if (cmd2 == "add") {
          // karing keys add [--role R] [--label L] [--disabled] [--json]
          bool want_json = false, want_disabled = false; std::string role = "write"; std::string label;
          for (int i=3; i<argc; ++i) {
            std::string a = argv[i];
            if (a == "--json") want_json = true;
            else if (a == "--disabled") want_disabled = true;
            else if (a == "--role" && i+1 < argc) { role = argv[++i]; }
            else if (a == "--label" && i+1 < argc) { label = argv[++i]; }
          }
          auto maybe = generate_secret_hex(); if (!maybe) { std::cerr << "random generation failed\n"; return 1; }
          const std::string secret = *maybe;
          sqlite3* dbh=nullptr; if (sqlite3_open_v2(resolvedDb.c_str(), &dbh, SQLITE_OPEN_READWRITE, nullptr)!=SQLITE_OK) return 1;
          sqlite3_stmt* st=nullptr; const char* ins="INSERT INTO api_keys(key,label,enabled,role,created_at) VALUES(?,?,?,?,strftime('%s','now'));";
          if (sqlite3_prepare_v2(dbh, ins, -1, &st, nullptr)==SQLITE_OK) {
            sqlite3_bind_text(st,1,secret.c_str(),-1,SQLITE_TRANSIENT);
            if (!label.empty()) sqlite3_bind_text(st,2,label.c_str(),-1,SQLITE_TRANSIENT); else sqlite3_bind_null(st,2);
            sqlite3_bind_int(st,3, want_disabled?0:1);
            sqlite3_bind_text(st,4,role.c_str(),-1,SQLITE_TRANSIENT);
            sqlite3_step(st);
          }
          if (st) sqlite3_finalize(st);
          long long new_id = sqlite3_last_insert_rowid(dbh);
          sqlite3_close(dbh);
          if (want_json) {
            std::cout << "{\"id\":" << new_id
                      << ",\"role\":\"" << role << "\""
                      << ",\"label\":\"" << label << "\""
                      << ",\"enabled\":" << (want_disabled?0:1)
                      << ",\"secret\":\"" << secret << "\"}" << std::endl;
          } else {
            std::cout << secret << std::endl; // secret shown once
          }
          return 0;
        } else if (cmd2 == "set-role" && argc >= 5) {
          // karing keys set-role <id> <role>
          int id = std::atoi(argv[3]); std::string role = argv[4];
          sqlite3* dbh=nullptr; if (sqlite3_open_v2(resolvedDb.c_str(), &dbh, SQLITE_OPEN_READWRITE, nullptr)!=SQLITE_OK) return 1;
          sqlite3_stmt* st=nullptr; const char* upd="UPDATE api_keys SET role=? WHERE id=?;";
          if (sqlite3_prepare_v2(dbh, upd, -1, &st, nullptr)==SQLITE_OK) { sqlite3_bind_text(st,1,role.c_str(),-1,SQLITE_TRANSIENT); sqlite3_bind_int(st,2,id); sqlite3_step(st);} if (st) sqlite3_finalize(st); sqlite3_close(dbh);
          std::cout << "role updated\n"; return 0;
        } else if (cmd2 == "set-label" && argc >= 5) {
          // karing keys set-label <id> <label>
          int id = std::atoi(argv[3]); std::string label = argv[4];
          sqlite3* dbh=nullptr; if (sqlite3_open_v2(resolvedDb.c_str(), &dbh, SQLITE_OPEN_READWRITE, nullptr)!=SQLITE_OK) return 1;
          sqlite3_stmt* st=nullptr; const char* upd="UPDATE api_keys SET label=? WHERE id=?;";
          if (sqlite3_prepare_v2(dbh, upd, -1, &st, nullptr)==SQLITE_OK) { if (!label.empty()) sqlite3_bind_text(st,1,label.c_str(),-1,SQLITE_TRANSIENT); else sqlite3_bind_null(st,1); sqlite3_bind_int(st,2,id); sqlite3_step(st);} if (st) sqlite3_finalize(st); sqlite3_close(dbh);
          std::cout << "label updated\n"; return 0;
        } else if (cmd2 == "disable" && argc >= 4) {
          // karing keys disable <id>
          int id = std::atoi(argv[3]);
          sqlite3* dbh=nullptr; if (sqlite3_open_v2(resolvedDb.c_str(), &dbh, SQLITE_OPEN_READWRITE, nullptr)!=SQLITE_OK) return 1;
          sqlite3_stmt* st=nullptr; const char* upd="UPDATE api_keys SET enabled=0 WHERE id=?;";
          if (sqlite3_prepare_v2(dbh, upd, -1, &st, nullptr)==SQLITE_OK) { sqlite3_bind_int(st,1,id); sqlite3_step(st);} if (st) sqlite3_finalize(st); sqlite3_close(dbh);
          std::cout << "disabled\n"; return 0;
        } else if (cmd2 == "enable" && argc >= 4) {
          // karing keys enable <id>
          int id = std::atoi(argv[3]);
          sqlite3* dbh=nullptr; if (sqlite3_open_v2(resolvedDb.c_str(), &dbh, SQLITE_OPEN_READWRITE, nullptr)!=SQLITE_OK) return 1;
          sqlite3_stmt* st=nullptr; const char* upd="UPDATE api_keys SET enabled=1 WHERE id=?;";
          if (sqlite3_prepare_v2(dbh, upd, -1, &st, nullptr)==SQLITE_OK) { sqlite3_bind_int(st,1,id); sqlite3_step(st);} if (st) sqlite3_finalize(st); sqlite3_close(dbh);
          std::cout << "enabled\n"; return 0;
        } else if (cmd2 == "rm" && argc >= 4) {
          // karing keys rm <id> [--hard]
          int id = std::atoi(argv[3]);
          bool hard = false; for (int i=4;i<argc;++i) if (std::string(argv[i])=="--hard") hard = true;
          sqlite3* dbh=nullptr; if (sqlite3_open_v2(resolvedDb.c_str(), &dbh, SQLITE_OPEN_READWRITE, nullptr)!=SQLITE_OK) return 1;
          sqlite3_stmt* st=nullptr;
          const char* sql = hard? "DELETE FROM api_keys WHERE id=?;" : "UPDATE api_keys SET enabled=0 WHERE id=?;";
          if (sqlite3_prepare_v2(dbh, sql, -1, &st, nullptr)==SQLITE_OK) { sqlite3_bind_int(st,1,id); sqlite3_step(st);} if (st) sqlite3_finalize(st); sqlite3_close(dbh);
          std::cout << (hard?"deleted\n":"disabled\n"); return 0;
        }
      }
      // usage fallthrough prints help via legacy flags below
    } else if (cmd1 == "ip") {
      if (argc >= 3) {
        std::string cmd2 = argv[2];
        if (cmd2 == "add" && argc >= 5) {
          // karing ip add <cidr_or_ip> allow|deny
          std::string raw = argv[3]; std::string list = argv[4];
          std::string to_store = normalize_ipv4_cidr(raw);
          const char* table = (list == "allow")? "ip_allow" : (list == "deny" ? "ip_deny" : nullptr);
          if (!table) { std::cerr << "ip list must be allow|deny\n"; return 1; }
          sqlite3* dbh=nullptr; if (sqlite3_open_v2(resolvedDb.c_str(), &dbh, SQLITE_OPEN_READWRITE, nullptr)!=SQLITE_OK) return 1;
          sqlite3_stmt* st=nullptr; std::string ins = std::string("INSERT OR IGNORE INTO ") + table + "(cidr, enabled, created_at) VALUES(?,1,strftime('%s','now'));";
          if (sqlite3_prepare_v2(dbh, ins.c_str(), -1, &st, nullptr)==SQLITE_OK) { sqlite3_bind_text(st,1,to_store.c_str(),-1,SQLITE_TRANSIENT); sqlite3_step(st);} if (st) sqlite3_finalize(st); sqlite3_close(dbh);
          std::cout << "ok\n"; return 0;
        } else if (cmd2 == "rm" && argc >= 4) {
          // karing ip rm allow:<id> | deny:<id>
          std::string spec = argv[3];
          auto pos = spec.find(':'); if (pos==std::string::npos) { std::cerr << "use allow:<id> or deny:<id>\n"; return 1; }
          std::string list = spec.substr(0,pos); int id = std::atoi(spec.substr(pos+1).c_str());
          const char* table = (list == "allow")? "ip_allow" : (list == "deny" ? "ip_deny" : nullptr);
          if (!table) { std::cerr << "ip list must be allow|deny\n"; return 1; }
          sqlite3* dbh=nullptr; if (sqlite3_open_v2(resolvedDb.c_str(), &dbh, SQLITE_OPEN_READWRITE, nullptr)!=SQLITE_OK) return 1;
          sqlite3_stmt* st=nullptr; std::string del = std::string("DELETE FROM ") + table + " WHERE id=?;";
          if (sqlite3_prepare_v2(dbh, del.c_str(), -1, &st, nullptr)==SQLITE_OK) { sqlite3_bind_int(st,1,id); sqlite3_step(st);} if (st) sqlite3_finalize(st); sqlite3_close(dbh);
          std::cout << "ok\n"; return 0;
        }
      }
      // usage fallthrough prints help via legacy flags below
    }
  }
  if (const char* env = std::getenv("KARING_SHOW_API_KEYS"); (env && *env) || hasArg("--show-api-keys")) {
    sqlite3* dbh=nullptr; if (sqlite3_open_v2(resolvedDb.c_str(), &dbh, SQLITE_OPEN_READONLY, nullptr)!=SQLITE_OK) return 1;
    const char* q="SELECT id, key, label, enabled, created_at, last_used_at, last_ip FROM api_keys ORDER BY id;";
    sqlite3_stmt* st=nullptr; if (sqlite3_prepare_v2(dbh, q, -1, &st, nullptr)==SQLITE_OK) {
      std::cout << "id\tkey\tlabel\tenabled\tcreated_at\tlast_used_at\tlast_ip\n";
      while (sqlite3_step(st)==SQLITE_ROW) {
        std::cout << sqlite3_column_int(st,0) << "\t"
                  << (const char*)sqlite3_column_text(st,1) << "\t"
                  << (const char*)(sqlite3_column_text(st,2)?sqlite3_column_text(st,2): (const unsigned char*)"") << "\t"
                  << sqlite3_column_int(st,3) << "\t"
                  << sqlite3_column_int64(st,4) << "\t"
                  << (sqlite3_column_type(st,5)!=SQLITE_NULL? std::to_string(sqlite3_column_int64(st,5)) : std::string("")) << "\t"
                  << (const char*)(sqlite3_column_text(st,6)?sqlite3_column_text(st,6): (const unsigned char*)"")
                  << "\n";
      }
    }
    if (st) sqlite3_finalize(st); sqlite3_close(dbh); return 0;
  }

  if (hasArg("--api-key-add")) {
    auto key = argValue("--api-key-add"); if (!key) { std::cerr<<"--api-key-add <key> required\n"; return 1; }
    auto label = argValue("--label"); auto role = argValue("--role"); std::string r = role?*role:"write";
    sqlite3* dbh=nullptr; if (sqlite3_open_v2(resolvedDb.c_str(), &dbh, SQLITE_OPEN_READWRITE, nullptr)!=SQLITE_OK) return 1;
    sqlite3_stmt* st=nullptr; const char* ins="INSERT INTO api_keys(key,label,enabled,role,created_at) VALUES(?,?,1,?,strftime('%s','now'));";
    if (sqlite3_prepare_v2(dbh, ins, -1, &st, nullptr)==SQLITE_OK) {
      sqlite3_bind_text(st,1,key->c_str(),-1,SQLITE_TRANSIENT);
      if (label && !label->empty()) sqlite3_bind_text(st,2,label->c_str(),-1,SQLITE_TRANSIENT); else sqlite3_bind_null(st,2);
      sqlite3_bind_text(st,3,r.c_str(),-1,SQLITE_TRANSIENT);
      sqlite3_step(st);
    }
    if (st) sqlite3_finalize(st); sqlite3_close(dbh); std::cout<<"added\n"; return 0;
  }
  if (hasArg("--api-key-disable") || hasArg("--api-key-enable")) {
    bool enable = hasArg("--api-key-enable"); auto key = argValue(enable?"--api-key-enable":"--api-key-disable"); if(!key){ std::cerr<<"key required\n"; return 1; }
    sqlite3* dbh=nullptr; if (sqlite3_open_v2(resolvedDb.c_str(), &dbh, SQLITE_OPEN_READWRITE, nullptr)!=SQLITE_OK) return 1;
    sqlite3_stmt* st=nullptr; const char* upd="UPDATE api_keys SET enabled=? WHERE key=?;";
    if (sqlite3_prepare_v2(dbh, upd, -1, &st, nullptr)==SQLITE_OK) {
      sqlite3_bind_int(st,1, enable?1:0);
      sqlite3_bind_text(st,2,key->c_str(),-1,SQLITE_TRANSIENT);
      sqlite3_step(st);
    }
    if (st) sqlite3_finalize(st); sqlite3_close(dbh); std::cout<<(enable?"enabled\n":"disabled\n"); return 0;
  }

  if (hasArg("--api-key-set-role")) {
    auto key = argValue("--api-key-set-role"); auto role = argValue("--role"); if (!key || !role) { std::cerr<<"--api-key-set-role <key> --role <read|write|admin>\n"; return 1; }
    sqlite3* dbh=nullptr; if (sqlite3_open_v2(resolvedDb.c_str(), &dbh, SQLITE_OPEN_READWRITE, nullptr)!=SQLITE_OK) return 1;
    sqlite3_stmt* st=nullptr; const char* upd="UPDATE api_keys SET role=? WHERE key=?;";
    if (sqlite3_prepare_v2(dbh, upd, -1, &st, nullptr)==SQLITE_OK) { sqlite3_bind_text(st,1,role->c_str(),-1,SQLITE_TRANSIENT); sqlite3_bind_text(st,2,key->c_str(),-1,SQLITE_TRANSIENT); sqlite3_step(st);} if (st) sqlite3_finalize(st); sqlite3_close(dbh); std::cout<<"role updated\n"; return 0;
  }
  if (hasArg("--api-key-delete")) {
    auto key = argValue("--api-key-delete"); if (!key) { std::cerr<<"--api-key-delete <key>\n"; return 1; }
    sqlite3* dbh=nullptr; if (sqlite3_open_v2(resolvedDb.c_str(), &dbh, SQLITE_OPEN_READWRITE, nullptr)!=SQLITE_OK) return 1;
    sqlite3_stmt* st=nullptr; const char* del="DELETE FROM api_keys WHERE key=?;";
    if (sqlite3_prepare_v2(dbh, del, -1, &st, nullptr)==SQLITE_OK) { sqlite3_bind_text(st,1,key->c_str(),-1,SQLITE_TRANSIENT); sqlite3_step(st);} if (st) sqlite3_finalize(st); sqlite3_close(dbh); std::cout<<"deleted\n"; return 0;
  }

  if (hasArg("--api-key-issue")) {
    // Generate a random API key and insert with optional --label and --role
    unsigned char buf[24]; // 192-bit -> 32 hex chars + padding
    if (RAND_bytes(buf, sizeof(buf)) != 1) { std::cerr<<"RAND_bytes failed\n"; return 1; }
    static const char* hex = "0123456789abcdef";
    std::string key; key.resize(sizeof(buf) * 2);
    for (size_t i=0;i<sizeof(buf);++i) { key[i*2] = hex[(buf[i]>>4)&0xF]; key[i*2+1] = hex[buf[i]&0xF]; }
    auto label = argValue("--label"); auto role = argValue("--role"); std::string r = role?*role:"write";
    sqlite3* dbh=nullptr; if (sqlite3_open_v2(resolvedDb.c_str(), &dbh, SQLITE_OPEN_READWRITE, nullptr)!=SQLITE_OK) return 1;
    sqlite3_stmt* st=nullptr; const char* ins="INSERT INTO api_keys(key,label,enabled,role,created_at) VALUES(?,?,1,?,strftime('%s','now'));";
    if (sqlite3_prepare_v2(dbh, ins, -1, &st, nullptr)==SQLITE_OK) {
      sqlite3_bind_text(st,1,key.c_str(),-1,SQLITE_TRANSIENT);
      if (label && !label->empty()) sqlite3_bind_text(st,2,label->c_str(),-1,SQLITE_TRANSIENT); else sqlite3_bind_null(st,2);
      sqlite3_bind_text(st,3,r.c_str(),-1,SQLITE_TRANSIENT);
      sqlite3_step(st);
    }
    if (st) sqlite3_finalize(st); sqlite3_close(dbh);
    std::cout << key << "\n"; // print only key to stdout
    return 0;
  }

  if (hasArg("--show-ip-rules")) {
    sqlite3* dbh=nullptr; if (sqlite3_open_v2(resolvedDb.c_str(), &dbh, SQLITE_OPEN_READONLY, nullptr)!=SQLITE_OK) return 1;
    auto dump = [&](const char* table){
      std::string sql = std::string("SELECT cidr, enabled, created_at FROM ") + table + " ORDER BY id;";
      sqlite3_stmt* st=nullptr; if (sqlite3_prepare_v2(dbh, sql.c_str(), -1, &st, nullptr)==SQLITE_OK) {
        std::cout << table << "\n";
        std::cout << "cidr\tenabled\tcreated_at\n";
        while (sqlite3_step(st)==SQLITE_ROW) {
          std::cout << (const char*)sqlite3_column_text(st,0) << "\t"
                    << sqlite3_column_int(st,1) << "\t"
                    << sqlite3_column_int64(st,2) << "\n";
        }
      }
      if (st) sqlite3_finalize(st);
    };
    dump("ip_allow");
    dump("ip_deny");
    sqlite3_close(dbh);
    return 0;
  }

  if (hasArg("--ip-allow-add")) {
    auto cidr = argValue("--ip-allow-add"); if (!cidr) { std::cerr<<"--ip-allow-add <cidr_or_ip>\n"; return 1; }
    sqlite3* dbh=nullptr; if (sqlite3_open_v2(resolvedDb.c_str(), &dbh, SQLITE_OPEN_READWRITE, nullptr)!=SQLITE_OK) return 1;
    sqlite3_stmt* st=nullptr; const char* ins="INSERT OR IGNORE INTO ip_allow(cidr, enabled, created_at) VALUES(?,1,strftime('%s','now'));";
    if (sqlite3_prepare_v2(dbh, ins, -1, &st, nullptr)==SQLITE_OK) { sqlite3_bind_text(st,1,cidr->c_str(),-1,SQLITE_TRANSIENT); sqlite3_step(st);} if(st) sqlite3_finalize(st); sqlite3_close(dbh); std::cout<<"ok\n"; return 0;
  }
  if (hasArg("--ip-allow-del")) {
    auto cidr = argValue("--ip-allow-del"); if (!cidr) { std::cerr<<"--ip-allow-del <cidr_or_ip>\n"; return 1; }
    sqlite3* dbh=nullptr; if (sqlite3_open_v2(resolvedDb.c_str(), &dbh, SQLITE_OPEN_READWRITE, nullptr)!=SQLITE_OK) return 1;
    sqlite3_stmt* st=nullptr; const char* del="DELETE FROM ip_allow WHERE cidr=?;";
    if (sqlite3_prepare_v2(dbh, del, -1, &st, nullptr)==SQLITE_OK) { sqlite3_bind_text(st,1,cidr->c_str(),-1,SQLITE_TRANSIENT); sqlite3_step(st);} if(st) sqlite3_finalize(st); sqlite3_close(dbh); std::cout<<"ok\n"; return 0;
  }
  if (hasArg("--ip-deny-add")) {
    auto cidr = argValue("--ip-deny-add"); if (!cidr) { std::cerr<<"--ip-deny-add <cidr_or_ip>\n"; return 1; }
    sqlite3* dbh=nullptr; if (sqlite3_open_v2(resolvedDb.c_str(), &dbh, SQLITE_OPEN_READWRITE, nullptr)!=SQLITE_OK) return 1;
    sqlite3_stmt* st=nullptr; const char* ins="INSERT OR IGNORE INTO ip_deny(cidr, enabled, created_at) VALUES(?,1,strftime('%s','now'));";
    if (sqlite3_prepare_v2(dbh, ins, -1, &st, nullptr)==SQLITE_OK) { sqlite3_bind_text(st,1,cidr->c_str(),-1,SQLITE_TRANSIENT); sqlite3_step(st);} if(st) sqlite3_finalize(st); sqlite3_close(dbh); std::cout<<"ok\n"; return 0;
  }
  if (hasArg("--ip-deny-del")) {
    auto cidr = argValue("--ip-deny-del"); if (!cidr) { std::cerr<<"--ip-deny-del <cidr_or_ip>\n"; return 1; }
    sqlite3* dbh=nullptr; if (sqlite3_open_v2(resolvedDb.c_str(), &dbh, SQLITE_OPEN_READWRITE, nullptr)!=SQLITE_OK) return 1;
    sqlite3_stmt* st=nullptr; const char* del="DELETE FROM ip_deny WHERE cidr=?;";
    if (sqlite3_prepare_v2(dbh, del, -1, &st, nullptr)==SQLITE_OK) { sqlite3_bind_text(st,1,cidr->c_str(),-1,SQLITE_TRANSIENT); sqlite3_step(st);} if(st) sqlite3_finalize(st); sqlite3_close(dbh); std::cout<<"ok\n"; return 0;
  }

  // Convenience: --issue <arg>
  // If <arg> looks like an IP/CIDR (contains '.' or ':' or '/'), add to ip_allow.
  // Otherwise treat as role for API key issuance.
  if (hasArg("--issue")) {
    auto v = argValue("--issue"); if (!v) { std::cerr<<"--issue <role|cidr>\n"; return 1; }
    bool looks_ip = (v->find('.') != std::string::npos) || (v->find(':') != std::string::npos) || (v->find('/') != std::string::npos);
    if (looks_ip) {
      sqlite3* dbh=nullptr; if (sqlite3_open_v2(resolvedDb.c_str(), &dbh, SQLITE_OPEN_READWRITE, nullptr)!=SQLITE_OK) return 1;
      sqlite3_stmt* st=nullptr; const char* ins="INSERT OR IGNORE INTO ip_allow(cidr, enabled, created_at) VALUES(?,1,strftime('%s','now'));";
      if (sqlite3_prepare_v2(dbh, ins, -1, &st, nullptr)==SQLITE_OK) { sqlite3_bind_text(st,1,v->c_str(),-1,SQLITE_TRANSIENT); sqlite3_step(st);} if(st) sqlite3_finalize(st); sqlite3_close(dbh); std::cout<<"ok\n"; return 0;
    } else {
      // role
      unsigned char buf[24]; if (RAND_bytes(buf, sizeof(buf)) != 1) { std::cerr<<"RAND_bytes failed\n"; return 1; }
      static const char* hex = "0123456789abcdef";
      std::string key; key.resize(sizeof(buf) * 2);
      for (size_t i=0;i<sizeof(buf);++i) { key[i*2] = hex[(buf[i]>>4)&0xF]; key[i*2+1] = hex[buf[i]&0xF]; }
      auto label = argValue("--label");
      sqlite3* dbh=nullptr; if (sqlite3_open_v2(resolvedDb.c_str(), &dbh, SQLITE_OPEN_READWRITE, nullptr)!=SQLITE_OK) return 1;
      sqlite3_stmt* st=nullptr; const char* ins="INSERT INTO api_keys(key,label,enabled,role,created_at) VALUES(?,?,1,?,strftime('%s','now'));";
      if (sqlite3_prepare_v2(dbh, ins, -1, &st, nullptr)==SQLITE_OK) {
        sqlite3_bind_text(st,1,key.c_str(),-1,SQLITE_TRANSIENT);
        if (label && !label->empty()) sqlite3_bind_text(st,2,label->c_str(),-1,SQLITE_TRANSIENT); else sqlite3_bind_null(st,2);
        sqlite3_bind_text(st,3,v->c_str(),-1,SQLITE_TRANSIENT);
        sqlite3_step(st);
      }
      if (st) sqlite3_finalize(st); sqlite3_close(dbh);
      std::cout << key << "\n";
      return 0;
    }
  }

  // Pre-routing advice to map <base_path>/* to /* so the same controllers serve both
  {
    const auto base = karing::options::base_path();
    if (!base.empty() && base != "/") {
      std::string p = base;
      if (p.size()>1 && p.back()=='/') p.pop_back();
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

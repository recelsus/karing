#include "init/bootstrap.h"

#include <algorithm>
#include <filesystem>
#include <iostream>
#include <stdexcept>
#include <string>

#include <drogon/drogon.h>

#include "db/db_init.h"
#include "db/db_introspection.h"
#include "db/db_path.h"
#include "init/cli_output.h"
#include "utils/options.h"
#include "utils/limits.h"
#include "version.h"

namespace karing::app {

namespace {

int clamp_limit(int limit_override) {
  int limit_value = karing::limits::kDefaultLimit;
  if (limit_override > 0) limit_value = limit_override;
  if (limit_value > karing::limits::kMaxLimit) {
    LOG_WARN << "limit " << limit_value << " exceeds max " << karing::limits::kMaxLimit << "; clamping.";
    limit_value = karing::limits::kMaxLimit;
  }
  if (limit_value < 1) limit_value = 1;
  return limit_value;
}

long long clamp_size(long long override_value, long long hard_max, const char* label) {
  if (override_value < 1) {
    throw std::runtime_error(std::string(label) + " must be >= 1 MB");
  }
  if (override_value > hard_max) {
    throw std::runtime_error(std::string(label) + " exceeds max " + std::to_string(hard_max) + " MB");
  }
  return override_value * karing::limits::kBytesPerMb;
}

void print_startup_summary(const std::string& listen_address,
                           int listen_port,
                           const std::string& db_path,
                           const std::string& upload_path,
                           const std::string& log_path,
                           int limit_value,
                           int max_file_mb,
                           int max_text_mb) {
  std::cout << "karing-server " << KARING_VERSION << '\n';
  std::cout << "listen: " << listen_address << ':' << listen_port << '\n';
  std::cout << "db: " << db_path << '\n';
  std::cout << "upload: " << upload_path << '\n';
  std::cout << "log: " << log_path << '\n';
  std::cout << "limit: " << limit_value << "/" << karing::limits::kMaxLimit << '\n';
  std::cout << "max_file_mb: " << max_file_mb << "/" << karing::limits::kMaxFileMb << '\n';
  std::cout << "max_text_mb: " << max_text_mb << "/" << karing::limits::kMaxTextMb << '\n';
}

}  // namespace

bootstrap::bootstrap(int argc_value, char** argv_value)
    : argc_(argc_value), argv_(argv_value) {}

int bootstrap::execute() {
  namespace fs = std::filesystem;
  auto& current_options = karing::options::current();

  auto options = karing::options::parse(argc_, argv_);
  if (options.action_kind == karing::options::action::help) {
    init::print_help();
    return 0;
  }
  if (options.action_kind == karing::options::action::version) {
    init::print_version();
    return 0;
  }
  if (options.action_kind == karing::options::action::error) {
    std::cerr << options.error << "\n";
    return 2;
  }

  const bool create_db = !options.check_only;
  const auto db_path_result = karing::db::resolve_db_path(options.db_path, create_db);
  if (!db_path_result.ok) {
    LOG_ERROR << "Failed to resolve database path: " << db_path_result.error;
    return 1;
  }

  const auto& resolved_db = db_path_result.path;
  if (options.check_only) {
    const auto check = karing::db::inspect::check_schema(resolved_db);
    if (!check.ok) {
      LOG_ERROR << "database schema check failed: " << check.error;
      return 1;
    }
    std::cout << "db_path=" << resolved_db << "\n";
    std::cout << "schema=ok\n";
    return 0;
  }

  fs::path data_path = fs::path(db_path_result.path);
  {
    std::error_code ec;
    fs::current_path(data_path.parent_path(), ec);
    if (ec) {
      LOG_ERROR << "Failed to change working directory to '" << data_path.parent_path().string() << "': " << ec.message();
      return 1;
    }
  }

  const int limit_value = clamp_limit(options.limit);

  try {
    const std::string cfg_name = "karing.sqlite";
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
  } catch (...) {
  }

  const auto init_result = karing::db::init_sqlite_schema_file(resolved_db, limit_value, options.force);
  if (!init_result.ok) {
    LOG_ERROR << "failed to initialize sqlite schema: " << init_result.error;
    return 1;
  }

  try {
    fs::create_directories("logs");
  } catch (...) {
  }

  const std::string listen_address = options.listen_address.empty() ? "0.0.0.0" : options.listen_address;
  const int listen_port = options.port > 0 ? options.port : 8080;
  const int max_file_mb = options.max_file_bytes;
  const int max_text_mb = options.max_text_bytes;
  long long max_file_bytes = 0;
  long long max_text_bytes = 0;
  try {
    max_file_bytes = clamp_size(max_file_mb, karing::limits::kMaxFileMb, "max-file");
    max_text_bytes = clamp_size(max_text_mb, karing::limits::kMaxTextMb, "max-text");
  } catch (const std::exception& ex) {
    LOG_ERROR << ex.what();
    return 1;
  }

  drogon::app().addListener(listen_address, static_cast<uint16_t>(listen_port));

  std::string resolved_log_path;
  if (const char* env_log = std::getenv("KARING_LOG_PATH"); env_log && *env_log) {
    try {
      fs::create_directories(env_log);
    } catch (...) {
    }
    resolved_log_path = env_log;
    drogon::app().setLogPath(resolved_log_path);
  } else {
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
      resolved_log_path = state_home.string();
      drogon::app().setLogPath(resolved_log_path);
    }
  }

  if (options.base_path.empty()) options.base_path = "/";
  if (options.base_path.size() > 1 && options.base_path.back() == '/') options.base_path.pop_back();

  fs::path upload_path;
  if (!options.upload_path.empty()) {
    upload_path = fs::absolute(options.upload_path);
  }
  if (upload_path.empty()) {
    if (data_path.parent_path() == "/var/lib/karing") {
      if (const char* xdg = std::getenv("XDG_DATA_HOME"); xdg && *xdg) {
        upload_path = fs::path(xdg) / "karing" / "uploads";
      } else if (const char* home = std::getenv("HOME"); home && *home) {
        upload_path = fs::path(home) / ".local" / "share" / "karing" / "uploads";
      } else {
        upload_path = fs::current_path() / ".local" / "share" / "karing" / "uploads";
      }
    } else {
      upload_path = data_path.parent_path() / "uploads";
    }
  }
  {
    std::error_code ec;
    fs::create_directories(upload_path, ec);
    if (ec) {
      LOG_ERROR << "Failed to create upload directory '" << upload_path.string() << "': " << ec.message();
      return 1;
    }
  }

  options.db_path = resolved_db;
  options.limit = limit_value;
  options.max_file_bytes = static_cast<int>(max_file_bytes);
  options.max_text_bytes = static_cast<int>(max_text_bytes);
  options.listen_address = listen_address;
  options.port = listen_port;
  options.upload_path = upload_path.string();
  options.log_path = resolved_log_path;
  current_options = options;
  drogon::app().setClientMaxBodySize(static_cast<size_t>(max_file_bytes));

  print_startup_summary(listen_address,
                        listen_port,
                        resolved_db,
                        upload_path.string(),
                        resolved_log_path,
                        limit_value,
                        max_file_mb,
                        max_text_mb);

  if (options.init_only) {
    auto tables = karing::db::inspect::list_tables_with_sql(resolved_db);
    std::cout << "Tables (" << tables.size() << ")\n";
    for (const auto& table : tables) std::cout << "- " << table.first << "\n";
    std::cout << "listen=" << listen_address << ":" << listen_port << "\n";
    std::cout << "db_path=" << resolved_db << "\n";
    std::cout << "upload_path=" << upload_path.string() << "\n";
    std::cout << "limit=" << limit_value << " (max=" << karing::limits::kMaxLimit << ")\n";
    std::cout << "force=" << (options.force ? "true" : "false") << "\n";
    std::cout << "max_file_mb=" << max_file_mb << " (max=" << karing::limits::kMaxFileMb << ")\n";
    std::cout << "max_text_mb=" << max_text_mb << " (max=" << karing::limits::kMaxTextMb << ")\n";
    std::cout << "max_file_bytes=" << max_file_bytes << "\n";
    std::cout << "max_text_bytes=" << max_text_bytes << "\n";
    return 0;
  }

  {
    const auto base = current_options.base_path;
    if (!base.empty() && base != "/") {
      std::string normalized = base;
      if (normalized.size() > 1 && normalized.back() == '/') normalized.pop_back();
      drogon::app().registerPreRoutingAdvice(
          [normalized](const drogon::HttpRequestPtr& req,
                       drogon::AdviceCallback&&,
                       drogon::AdviceChainCallback&& next) {
            const auto& path = req->path();
            if (!normalized.empty() &&
                (path == normalized || (path.rfind(normalized, 0) == 0 &&
                                        (path.size() == normalized.size() || path[normalized.size()] == '/')))) {
              std::string rewritten = path.substr(normalized.size());
              if (rewritten.empty()) rewritten = "/";
              req->setPath(std::move(rewritten));
            }
            next();
          });
    }
  }

  drogon::app().run();
  return 0;
}

}  // namespace karing::app

#pragma once

#include <string>

#include <sqlite3.h>

namespace karing::db {

constexpr int kSqliteBusyTimeoutMs = 5000;

enum class sqlite_access {
  read_only,
  read_write,
  read_write_create,
};

enum class sqlite_error_kind {
  none,
  open_failed,
  configure_failed,
  busy,
  constraint,
  io,
  unknown,
};

struct sqlite_error {
  sqlite_error_kind kind{sqlite_error_kind::none};
  int code{SQLITE_OK};
  int extended_code{SQLITE_OK};
  std::string message;
};

struct sqlite_operation_result {
  bool ok{false};
  sqlite_error error;
};

class sqlite_connection {
 public:
  sqlite_connection() = default;
  sqlite_connection(const std::string& path, sqlite_access access);
  ~sqlite_connection();

  sqlite_connection(const sqlite_connection&) = delete;
  sqlite_connection& operator=(const sqlite_connection&) = delete;

  sqlite_connection(sqlite_connection&& other) noexcept;
  sqlite_connection& operator=(sqlite_connection&& other) noexcept;

  bool open(const std::string& path, sqlite_access access);
  void close();

  bool ok() const;
  sqlite3* get();
  operator sqlite3*();

  const sqlite_error& error() const;

 private:
  sqlite3* handle_{nullptr};
  sqlite_error error_;
};

sqlite_error make_sqlite_error(sqlite3* db, sqlite_error_kind fallback_kind);
sqlite_error_kind classify_sqlite_code(int code);

}  // namespace karing::db

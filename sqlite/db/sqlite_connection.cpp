#include "db/sqlite_connection.h"

#include <utility>

namespace karing::db {

namespace {

int to_open_flags(sqlite_access access) {
  switch (access) {
    case sqlite_access::read_only:
      return SQLITE_OPEN_READONLY;
    case sqlite_access::read_write:
      return SQLITE_OPEN_READWRITE;
    case sqlite_access::read_write_create:
      return SQLITE_OPEN_READWRITE | SQLITE_OPEN_CREATE;
  }
  return SQLITE_OPEN_READONLY;
}

bool exec_pragma(sqlite3* db, const char* sql) {
  char* errmsg = nullptr;
  const int rc = sqlite3_exec(db, sql, nullptr, nullptr, &errmsg);
  if (errmsg) sqlite3_free(errmsg);
  return rc == SQLITE_OK;
}

}  // namespace

sqlite_error_kind classify_sqlite_code(int code) {
  switch (code & 0xff) {
    case SQLITE_OK:
      return sqlite_error_kind::none;
    case SQLITE_BUSY:
    case SQLITE_LOCKED:
      return sqlite_error_kind::busy;
    case SQLITE_CONSTRAINT:
      return sqlite_error_kind::constraint;
    case SQLITE_IOERR:
    case SQLITE_CANTOPEN:
      return sqlite_error_kind::io;
    default:
      return sqlite_error_kind::unknown;
  }
}

sqlite_error make_sqlite_error(sqlite3* db, sqlite_error_kind fallback_kind) {
  sqlite_error error;
  error.code = db ? sqlite3_errcode(db) : SQLITE_ERROR;
  error.extended_code = db ? sqlite3_extended_errcode(db) : error.code;
  error.kind = classify_sqlite_code(error.extended_code);
  if (error.kind == sqlite_error_kind::unknown) error.kind = fallback_kind;
  error.message = db ? sqlite3_errmsg(db) : "sqlite error";
  return error;
}

sqlite_connection::sqlite_connection(const std::string& path, sqlite_access access) {
  open(path, access);
}

sqlite_connection::~sqlite_connection() {
  close();
}

sqlite_connection::sqlite_connection(sqlite_connection&& other) noexcept
    : handle_(std::exchange(other.handle_, nullptr)), error_(std::move(other.error_)) {}

sqlite_connection& sqlite_connection::operator=(sqlite_connection&& other) noexcept {
  if (this == &other) return *this;
  close();
  handle_ = std::exchange(other.handle_, nullptr);
  error_ = std::move(other.error_);
  return *this;
}

bool sqlite_connection::open(const std::string& path, sqlite_access access) {
  close();
  error_ = {};

  if (sqlite3_open_v2(path.c_str(), &handle_, to_open_flags(access), nullptr) != SQLITE_OK) {
    error_ = make_sqlite_error(handle_, sqlite_error_kind::open_failed);
    close();
    return false;
  }

  sqlite3_extended_result_codes(handle_, 1);
  sqlite3_busy_timeout(handle_, kSqliteBusyTimeoutMs);

  if (!exec_pragma(handle_, "PRAGMA foreign_keys = ON;")) {
    error_ = make_sqlite_error(handle_, sqlite_error_kind::configure_failed);
    close();
    return false;
  }

  if (access != sqlite_access::read_only) {
    if (!exec_pragma(handle_, "PRAGMA journal_mode = WAL;") ||
        !exec_pragma(handle_, "PRAGMA synchronous = NORMAL;")) {
      error_ = make_sqlite_error(handle_, sqlite_error_kind::configure_failed);
      close();
      return false;
    }
  }

  return true;
}

void sqlite_connection::close() {
  if (handle_) sqlite3_close(handle_);
  handle_ = nullptr;
}

bool sqlite_connection::ok() const {
  return handle_ != nullptr;
}

sqlite3* sqlite_connection::get() {
  return handle_;
}

sqlite_connection::operator sqlite3*() {
  return handle_;
}

const sqlite_error& sqlite_connection::error() const {
  return error_;
}

}  // namespace karing::db

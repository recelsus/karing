#include "backend/sqlite_backend.h"

#include <ctime>
#include <filesystem>
#include <iomanip>
#include <iostream>
#include <optional>
#include <sstream>
#include <string>
#include <vector>

#include <json/json.h>
#include <sqlite3.h>

#include "db/db_init.h"
#include "db/db_introspection.h"
#include "domain/entry_operations.h"
#include "common/error/app_error.h"
#include "utils/arg_utils.h"
#include "utils/io.h"

namespace karing::cli::backend {

namespace fs = std::filesystem;

namespace {

constexpr int kDefaultSqliteLimit = 100;

bool has_file_metadata(const karing::domain::EntryRecord& record) {
  return record.is_file || !record.filename.empty();
}

karing::domain::app_error validation_error(std::string message) {
  return karing::domain::make_error(karing::domain::error_category::validation,
                                    karing::domain::error_code::validation,
                                    std::move(message));
}

karing::domain::app_error backend_error(std::string message) {
  return karing::domain::make_error(karing::domain::error_category::backend,
                                    karing::domain::error_code::backend_unsupported,
                                    std::move(message));
}

karing::domain::app_error not_found_error(std::string message = "not found") {
  return karing::domain::make_error(karing::domain::error_category::not_found,
                                    karing::domain::error_code::not_found,
                                    std::move(message));
}

karing::domain::app_error database_error(std::string message, std::string detail) {
  return karing::domain::make_error(karing::domain::error_category::database,
                                    karing::domain::error_code::sqlite_unknown,
                                    std::move(message),
                                    std::move(detail));
}

karing::domain::app_error database_busy_error() {
  return karing::domain::make_error(karing::domain::error_category::unavailable,
                                    karing::domain::error_code::sqlite_busy,
                                    "database is busy");
}

bool is_busy_code(int code) {
  const int primary = code & 0xff;
  return primary == SQLITE_BUSY || primary == SQLITE_LOCKED;
}

bool database_is_busy(const std::string& db_path) {
  sqlite3* db = nullptr;
  if (sqlite3_open_v2(db_path.c_str(), &db, SQLITE_OPEN_READWRITE, nullptr) != SQLITE_OK) {
    const int code = db ? sqlite3_extended_errcode(db) : SQLITE_ERROR;
    if (db) sqlite3_close(db);
    return is_busy_code(code);
  }
  sqlite3_extended_result_codes(db, 1);
  sqlite3_busy_timeout(db, 0);

  char* errmsg = nullptr;
  const int rc = sqlite3_exec(db, "BEGIN IMMEDIATE;", nullptr, nullptr, &errmsg);
  if (errmsg) sqlite3_free(errmsg);
  if (rc == SQLITE_OK) {
    sqlite3_exec(db, "ROLLBACK;", nullptr, nullptr, nullptr);
    sqlite3_close(db);
    return false;
  }
  const int code = sqlite3_extended_errcode(db);
  sqlite3_close(db);
  return is_busy_code(code);
}

karing::domain::app_error database_operation_error(const sqlite_context& context,
                                                   std::string message,
                                                   std::string detail) {
  if (database_is_busy(context.db_path)) return database_busy_error();
  return database_error(std::move(message), std::move(detail));
}

int print_error(const sqlite_context& context, const karing::domain::app_error& error) {
  return karing::cli::utils::print_error(error, context.json_output, false);
}

int print_init_error(const karing::domain::app_error& error, bool json_output) {
  return karing::cli::utils::print_error(error, json_output, false);
}

karing::domain::entry_operations make_operations(const sqlite_context& context) {
  return karing::domain::entry_operations(context.db_path, kDefaultSqliteLimit);
}

int ensure_schema(const sqlite_context& context) {
  const auto check = karing::db::inspect::check_schema(context.db_path);
  if (!check.ok) return print_error(context, database_error("invalid sqlite database", check.error));
  return 0;
}

Json::Value record_to_json(const karing::domain::EntryRecord& record) {
  Json::Value out(Json::objectValue);
  out["id"] = record.id;
  out["is_file"] = record.is_file;
  if (!record.content.empty()) out["content"] = record.content;
  if (!record.filename.empty()) out["filename"] = record.filename;
  if (!record.mime.empty()) out["mime"] = record.mime;
  out["created_at"] = Json::Int64(record.created_at);
  if (record.updated_at.has_value()) out["updated_at"] = Json::Int64(*record.updated_at);
  return out;
}

Json::Value success_json(const Json::Value& data, const Json::Value& meta = Json::Value()) {
  Json::Value root(Json::objectValue);
  root["success"] = true;
  root["message"] = "OK";
  root["data"] = data;
  if (!meta.isNull()) root["meta"] = meta;
  return root;
}

Json::Value id_json(const char* message, int id) {
  Json::Value root(Json::objectValue);
  root["success"] = true;
  root["message"] = message;
  root["id"] = id;
  return root;
}

void print_json(const Json::Value& value) {
  Json::StreamWriterBuilder writer;
  writer["indentation"] = "";
  std::cout << Json::writeString(writer, value) << '\n';
}

std::string format_timestamp(long long value) {
  if (value <= 0) return "-";
  const std::time_t ts = static_cast<std::time_t>(value);
  std::tm tm{};
#if defined(_WIN32)
  if (gmtime_s(&tm, &ts) != 0) return "-";
#else
  if (gmtime_r(&ts, &tm) == nullptr) return "-";
#endif
  std::ostringstream out;
  out << std::put_time(&tm, "%Y-%m-%d %H:%M:%S");
  return out.str();
}

std::string record_type(const karing::domain::EntryRecord& record) {
  if (record.is_file) return record.mime.empty() ? "file" : record.mime;
  if (!record.filename.empty()) return "text(file)";
  return "text";
}

std::string record_display_content(const karing::domain::EntryRecord& record, bool full) {
  const std::string value = !record.filename.empty() ? record.filename : record.content;
  if (full || value.size() <= 48) return value;
  return value.substr(0, 45) + "...";
}

void print_record_metadata(const karing::domain::EntryRecord& record) {
  std::cout << "id: " << record.id << '\n';
  std::cout << "type: " << record_type(record) << '\n';
  if (!record.filename.empty()) std::cout << "filename: " << record.filename << '\n';
  if (!record.mime.empty()) std::cout << "mime: " << record.mime << '\n';
  std::cout << "created_at: " << format_timestamp(record.created_at) << '\n';
  if (record.updated_at.has_value()) std::cout << "updated_at: " << format_timestamp(*record.updated_at) << '\n';
}

void print_records_table(const std::vector<karing::domain::EntryRecord>& records, bool full) {
  constexpr int kIdWidth = 6;
  constexpr int kTypeWidth = 24;
  constexpr int kContentWidth = 48;
  constexpr int kCreatedWidth = 21;
  constexpr int kUpdatedWidth = 21;

  std::cout << std::left
            << std::setw(kIdWidth) << "id"
            << std::setw(kTypeWidth) << "type"
            << std::setw(kContentWidth) << "content"
            << std::setw(kCreatedWidth) << "created_at"
            << std::setw(kUpdatedWidth) << "updated_at"
            << '\n';
  std::cout << std::string(kIdWidth + kTypeWidth + kContentWidth + kCreatedWidth + kUpdatedWidth, '-') << '\n';

  for (const auto& record : records) {
    std::cout << std::left
              << std::setw(kIdWidth) << record.id
              << std::setw(kTypeWidth) << record_type(record)
              << std::setw(kContentWidth) << record_display_content(record, full)
              << std::setw(kCreatedWidth) << format_timestamp(record.created_at)
              << std::setw(kUpdatedWidth) << (record.updated_at.has_value() ? format_timestamp(*record.updated_at) : "-")
              << '\n';
  }
}

std::optional<int> parse_id(const sqlite_context& context, const std::string& value, const char* label) {
  if (!karing::cli::utils::is_valid_id(value)) {
    print_error(context, validation_error(std::string(label) + " must be in range 1..1000"));
    return std::nullopt;
  }
  return std::stoi(value);
}

}  // namespace

int run_sqlite_get(const sqlite_context& context, std::optional<int> id) {
  if (const int status = ensure_schema(context); status != 0) return status;
  const auto operations = make_operations(context);
  const auto record = id.has_value() ? operations.record_by_id(*id) : operations.latest_record();
  if (!record.has_value()) return print_error(context, not_found_error());

  if (context.json_output) {
    Json::Value data(Json::arrayValue);
    data.append(record_to_json(*record));
    print_json(success_json(data));
    return 0;
  }

  if (has_file_metadata(*record)) {
    print_record_metadata(*record);
    return 0;
  }

  std::cout << record->content;
  return 0;
}

int run_sqlite_add(const sqlite_context& context, const std::vector<std::string>& args) {
  if (const int status = ensure_schema(context); status != 0) return status;
  std::vector<std::string> text_parts;
  for (size_t i = 0; i < args.size(); ++i) {
    const auto& arg = args[i];
    if (arg == "-f" || arg == "--file") return print_error(context, backend_error("sqlite backend does not support file upload"));
    if (arg == "--mime" || arg == "--name") return print_error(context, backend_error(arg + " is only available with file upload"));
    text_parts.push_back(arg);
  }

  std::string content = karing::cli::utils::join_words(text_parts);
  if (content.empty()) {
    if (const auto stdin_text = karing::cli::utils::read_stdin_text(); stdin_text.has_value()) content = *stdin_text;
  }
  if (content.empty()) return print_error(context, validation_error("text content is required"));

  const int id = make_operations(context).create_text(content);
  if (id < 0) return print_error(context, database_operation_error(context, "insert failed", "create_text returned a negative id"));
  if (context.json_output) print_json(id_json("Created", id));
  else std::cout << "created id: " << id << '\n';
  return 0;
}

int run_sqlite_delete(const sqlite_context& context, const std::vector<std::string>& args) {
  if (const int status = ensure_schema(context); status != 0) return status;
  if (args.size() > 1) return print_error(context, validation_error("del accepts at most one id"));

  auto operations = make_operations(context);
  std::optional<karing::domain::EntryRecord> record;
  if (args.empty()) {
    record = operations.latest_record();
  } else {
    const auto id = parse_id(context, args.front(), "delete id");
    if (!id.has_value()) return 1;
    record = operations.record_by_id(*id);
  }
  if (!record.has_value()) return print_error(context, not_found_error());
  if (has_file_metadata(*record)) return print_error(context, backend_error("sqlite backend does not support deleting file records"));

  const bool ok = args.empty() ? operations.delete_latest_recent(600) : operations.delete_by_id(record->id);
  if (!ok && args.empty()) return print_error(context, not_found_error("No recent latest record to delete"));
  if (!ok) return print_error(context, database_operation_error(context, "delete failed", "entry operation returned false"));
  if (context.json_output) print_json(id_json("OK", record->id));
  return 0;
}

int run_sqlite_find(const sqlite_context& context, const std::vector<std::string>& args) {
  if (const int status = ensure_schema(context); status != 0) return status;
  karing::domain::search_request request;
  std::vector<std::string> terms;
  bool asc = false;
  bool desc = false;
  bool full = false;

  for (size_t i = 0; i < args.size(); ++i) {
    const auto& arg = args[i];
    if ((arg == "--limit" || arg == "-l") && i + 1 < args.size()) {
      try {
        request.limit = std::stoi(args[++i]);
      } catch (...) {
        return print_error(context, validation_error("limit must be an integer"));
      }
      continue;
    }
    if ((arg == "--type" || arg == "-t") && i + 1 < args.size()) {
      request.type = args[++i];
      continue;
    }
    if ((arg == "--sort" || arg == "-s") && i + 1 < args.size()) {
      request.sort = args[++i];
      if (request.sort == "store") request.sort = "stored_at";
      else if (request.sort == "update") request.sort = "updated_at";
      continue;
    }
    if (arg == "--asc") {
      asc = true;
      continue;
    }
    if (arg == "--desc") {
      desc = true;
      continue;
    }
    if (arg == "--full") {
      full = true;
      continue;
    }
    terms.push_back(arg);
  }

  if (asc && desc) return print_error(context, validation_error("--asc and --desc cannot be used together"));
  request.order = asc ? "asc" : "desc";
  request.q = karing::cli::utils::join_words(terms);

  const auto result = make_operations(context).search(request);
  if (result.error == karing::domain::search_error::invalid_sort) return print_error(context, validation_error("invalid sort"));
  if (result.error == karing::domain::search_error::invalid_order) return print_error(context, validation_error("invalid order"));
  if (result.error == karing::domain::search_error::invalid_query) {
    return print_error(context, karing::domain::make_error(karing::domain::error_category::query,
                                                           karing::domain::error_code::query,
                                                           "invalid search query",
                                                           result.detail_reason.value_or("unknown")));
  }
  if (result.error == karing::domain::search_error::fts_unavailable) {
    return print_error(context, karing::domain::make_error(karing::domain::error_category::unavailable,
                                                           karing::domain::error_code::fts_unavailable,
                                                           "full-text search unavailable"));
  }

  if (context.json_output) {
    Json::Value data(Json::arrayValue);
    for (const auto& record : result.records) data.append(record_to_json(record));
    Json::Value meta(Json::objectValue);
    meta["count"] = static_cast<int>(result.records.size());
    meta["limit"] = result.limit;
    meta["sort"] = result.sort;
    meta["order"] = result.order;
    if (result.has_total) meta["total"] = Json::Int64(result.total);
    print_json(success_json(data, meta));
    return 0;
  }

  print_records_table(result.records, full);
  return 0;
}

int run_sqlite_mod(const sqlite_context& context, const std::vector<std::string>& args) {
  if (const int status = ensure_schema(context); status != 0) return status;
  if (args.empty()) return print_error(context, validation_error("mod requires an id"));
  const auto id = parse_id(context, args.front(), "mod id");
  if (!id.has_value()) return 1;

  std::vector<std::string> text_parts;
  for (size_t i = 1; i < args.size(); ++i) {
    const auto& arg = args[i];
    if (arg == "-f" || arg == "--file") return print_error(context, backend_error("sqlite backend does not support file replace"));
    if (arg == "--mime" || arg == "--name") return print_error(context, backend_error(arg + " is only available with file replace"));
    text_parts.push_back(arg);
  }

  std::string content = karing::cli::utils::join_words(text_parts);
  if (content.empty()) {
    if (const auto stdin_text = karing::cli::utils::read_stdin_text(); stdin_text.has_value()) content = *stdin_text;
  }
  if (content.empty()) return print_error(context, validation_error("text content is required"));

  auto operations = make_operations(context);
  const auto record = operations.record_by_id(*id);
  if (!record.has_value()) return print_error(context, not_found_error());
  if (has_file_metadata(*record)) return print_error(context, backend_error("sqlite backend does not support modifying file records"));
  if (!operations.replace_text(*id, content)) return print_error(context, database_operation_error(context, "update failed", "replace_text returned false"));

  if (context.json_output) print_json(id_json("OK", *id));
  else std::cout << "updated id: " << *id << '\n';
  return 0;
}

int run_sqlite_swap(const sqlite_context& context, const std::vector<std::string>& args) {
  if (const int status = ensure_schema(context); status != 0) return status;
  if (args.size() != 2) return print_error(context, validation_error("swap requires two ids"));
  const auto id1 = parse_id(context, args[0], "swap id");
  const auto id2 = parse_id(context, args[1], "swap id");
  if (!id1.has_value() || !id2.has_value()) return 1;
  if (*id1 == *id2) return print_error(context, validation_error("swap ids must be different"));

  const auto swapped = make_operations(context).swap(*id1, *id2);
  if (!swapped.has_value()) return print_error(context, database_operation_error(context, "swap failed", "swap returned no records"));

  if (context.json_output) {
    Json::Value data(Json::arrayValue);
    data.append(record_to_json(swapped->first));
    data.append(record_to_json(swapped->second));
    print_json(success_json(data));
  } else {
    std::cout << "swap complete\n";
  }
  return 0;
}

int run_sqlite_move(const sqlite_context& context, const std::vector<std::string>& args) {
  if (const int status = ensure_schema(context); status != 0) return status;
  if (args.size() != 2) return print_error(context, validation_error("move requires an id and a before-id"));
  const auto id = parse_id(context, args[0], "move id");
  const auto before_id = parse_id(context, args[1], "move before id");
  if (!id.has_value() || !before_id.has_value()) return 1;
  if (*id == *before_id) return print_error(context, validation_error("move ids must be different"));

  const auto moved = make_operations(context).move_before(*id, *before_id);
  if (!moved.has_value()) {
    if (database_is_busy(context.db_path)) return print_error(context, database_busy_error());
    return print_error(context, not_found_error("Move failed"));
  }

  if (context.json_output) {
    Json::Value data(Json::arrayValue);
    for (const auto& record : moved->first) data.append(record_to_json(record));
    Json::Value meta(Json::objectValue);
    meta["count"] = static_cast<int>(moved->first.size());
    meta["next_id"] = moved->second;
    print_json(success_json(data, meta));
  } else {
    std::cout << "move complete\n";
    std::cout << "count: " << moved->first.size() << '\n';
    std::cout << "next_id: " << moved->second << '\n';
  }
  return 0;
}

int run_sqlite_resequence(const sqlite_context& context) {
  if (const int status = ensure_schema(context); status != 0) return status;
  const auto resequenced = make_operations(context).resequence();
  if (!resequenced.has_value()) return print_error(context, database_operation_error(context, "resequence failed", "resequence returned no records"));

  if (context.json_output) {
    Json::Value data(Json::arrayValue);
    for (const auto& record : resequenced->first) data.append(record_to_json(record));
    Json::Value meta(Json::objectValue);
    meta["count"] = static_cast<int>(resequenced->first.size());
    meta["next_id"] = resequenced->second;
    print_json(success_json(data, meta));
  } else {
    std::cout << "resequence complete\n";
    std::cout << "count: " << resequenced->first.size() << '\n';
    std::cout << "next_id: " << resequenced->second << '\n';
  }
  return 0;
}

int run_sqlite_init_database(const std::vector<std::string>& args, bool json_output) {
  std::optional<std::string> db_path;
  int limit = kDefaultSqliteLimit;
  bool force = false;

  for (size_t i = 0; i < args.size(); ++i) {
    const auto& arg = args[i];
    if ((arg == "--limit" || arg == "-l") && i + 1 < args.size()) {
      try {
        limit = std::stoi(args[++i]);
      } catch (...) {
        return print_init_error(validation_error("limit must be an integer"), json_output);
      }
      continue;
    }
    if (arg == "--force") {
      force = true;
      continue;
    }
    if (!db_path.has_value()) {
      db_path = arg;
      continue;
    }
    return print_init_error(validation_error("init-db accepts exactly one path"), json_output);
  }

  if (!db_path.has_value() || db_path->empty()) {
    return print_init_error(validation_error("init-db requires a sqlite path"), json_output);
  }
  if (limit < 1 || limit > 1000) {
    return print_init_error(validation_error("limit must be in range 1..1000"), json_output);
  }

  const fs::path path = fs::absolute(*db_path);
  if (fs::exists(path) && !force) {
    return print_init_error(karing::domain::make_error(karing::domain::error_category::conflict,
                                                       karing::domain::error_code::conflict,
                                                       "sqlite database already exists; use --force to reinitialize or resize"),
                            json_output);
  }

  std::error_code ec;
  if (!path.parent_path().empty()) {
    fs::create_directories(path.parent_path(), ec);
    if (ec) return print_init_error(database_error("failed to create parent directory", ec.message()), json_output);
  }

  const auto result = karing::db::init_sqlite_schema_file(path.string(), limit, force);
  if (!result.ok) return print_init_error(database_error("sqlite init failed", result.error), json_output);

  if (json_output) {
    Json::Value root(Json::objectValue);
    root["success"] = true;
    root["message"] = result.created ? "Created" : "OK";
    root["db_path"] = path.string();
    root["created"] = result.created;
    root["resized"] = result.resized;
    root["previous_max_items"] = result.previous_max_items;
    root["current_max_items"] = result.current_max_items;
    print_json(root);
  } else {
    std::cout << "sqlite initialized\n";
    std::cout << "db: " << path.string() << '\n';
    std::cout << "max_items: " << result.current_max_items << '\n';
    std::cout << "created: " << (result.created ? "true" : "false") << '\n';
    std::cout << "resized: " << (result.resized ? "true" : "false") << '\n';
  }
  return 0;
}

}  // namespace karing::cli::backend

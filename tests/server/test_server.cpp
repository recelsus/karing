#include <algorithm>
#include <chrono>
#include <filesystem>
#include <functional>
#include <iostream>
#include <stdexcept>
#include <string>
#include <vector>

#include <drogon/HttpRequest.h>
#include <drogon/HttpResponse.h>
#include <json/json.h>
#include <sqlite3.h>

#if !defined(_WIN32)
#include <arpa/inet.h>
#include <netinet/in.h>
#include <sys/socket.h>
#include <unistd.h>
#endif

#include "controllers/health_controller.h"
#include "controllers/karing_root_controller.h"
#include "controllers/karing_search_controller.h"
#include "controllers/karing_search_live_controller.h"
#include "dao/karing_dao.h"
#include "db/db_init.h"
#include "db/sqlite_connection.h"
#include "common/error/app_error.h"
#include "utils/base_path.h"
#include "utils/json_response.h"
#include "utils/listen_probe.h"
#include "utils/upload_mime.h"
#include "utils/limits.h"
#include "utils/options.h"

namespace fs = std::filesystem;

namespace {

struct test_failure : std::runtime_error {
  using std::runtime_error::runtime_error;
};

struct temp_env {
  fs::path root;
  fs::path db_path;
  fs::path upload_path;
};

void expect(bool condition, const std::string& message) {
  if (!condition) throw test_failure(message);
}

temp_env make_temp_env(const std::string& name) {
  const auto now = std::chrono::steady_clock::now().time_since_epoch().count();
  temp_env env;
  env.root = fs::temp_directory_path() / ("karing-server-test-" + name + "-" + std::to_string(now));
  env.db_path = env.root / "karing.sqlite";
  env.upload_path = env.root / "uploads";
  fs::create_directories(env.upload_path);
  return env;
}

void set_current_options(const temp_env& env) {
  auto& options = karing::options::current();
  options = {};
  options.db_path = env.db_path.string();
  options.upload_path = env.upload_path.string();
  options.log_path = (env.root / "logs").string();
  options.listen_address = "127.0.0.1";
  options.port = 8080;
  options.limit = 25;
  options.max_file_bytes = 10 * karing::limits::kBytesPerMb;
  options.max_text_bytes = 1 * karing::limits::kBytesPerMb;
  options.base_path = "/karing";
}

drogon::HttpResponsePtr invoke(const std::function<void(std::function<void(const drogon::HttpResponsePtr&)>&&)>& fn) {
  drogon::HttpResponsePtr response;
  fn([&](const drogon::HttpResponsePtr& resp) { response = resp; });
  expect(response != nullptr, "controller did not produce a response");
  return response;
}

Json::Value response_json(const drogon::HttpResponsePtr& response) {
  const auto& object = response->getJsonObject();
  expect(static_cast<bool>(object), "response JSON object is missing");
  return *object;
}

drogon::HttpRequestPtr make_json_request(drogon::HttpMethod method, const Json::Value& body) {
  Json::StreamWriterBuilder builder;
  builder["indentation"] = "";
  auto req = drogon::HttpRequest::newHttpRequest();
  req->setMethod(method);
  req->addHeader("content-type", "application/json");
  req->setContentTypeString("application/json");
  req->setBody(Json::writeString(builder, body));
  return req;
}

void test_options_parse_modes() {
  {
    char arg0[] = "karing";
    char* argv[] = {arg0};
    auto parsed = karing::options::parse(1, argv);
    expect(parsed.listen_address == "127.0.0.1", "default listen address should be loopback");
  }

  {
    char arg0[] = "karing";
    char arg1[] = "--error-detail";
    char* argv[] = {arg0, arg1};
    auto parsed = karing::options::parse(2, argv);
    expect(parsed.show_error_details, "--error-detail should enable detailed errors");
  }

  {
    char arg0[] = "karing";
    char arg1[] = "--check-db";
    char* argv[] = {arg0, arg1};
    auto parsed = karing::options::parse(2, argv);
    expect(parsed.check_only, "--check-db should set check_only");
    expect(!parsed.init_only, "--check-db should not set init_only");
  }

  {
    char arg0[] = "karing";
    char arg1[] = "--init-db";
    char arg2[] = "--force";
    char* argv[] = {arg0, arg1, arg2};
    auto parsed = karing::options::parse(3, argv);
    expect(parsed.init_only, "--init-db should set init_only");
    expect(parsed.force, "--force should be parsed");
  }

  {
    char arg0[] = "karing";
    char arg1[] = "--check-db";
    char arg2[] = "--init-db";
    char* argv[] = {arg0, arg1, arg2};
    auto parsed = karing::options::parse(3, argv);
    expect(parsed.action_kind == karing::options::action::error, "conflicting db modes should be rejected");
  }
}

void test_common_http_error_response_hides_and_shows_detail() {
  const auto error = karing::domain::make_error(karing::domain::error_category::database,
                                                karing::domain::error_code::sqlite_io,
                                                "Database unavailable",
                                                "disk I/O detail");

  const auto hidden = karing::http::error(drogon::k500InternalServerError, error, false);
  const auto hidden_json = response_json(hidden);
  expect(hidden_json["success"].asBool() == false, "common HTTP error should mark failure");
  expect(hidden_json["code"].asString() == "E_SQLITE_IO", "common HTTP error should expose stable code");
  expect(hidden_json["message"].asString() == "Database unavailable", "common HTTP error should expose user message");
  expect(!hidden_json.isMember("details"), "common HTTP error should hide detail by default");

  const auto shown = karing::http::error(drogon::k500InternalServerError, error, true);
  const auto shown_json = response_json(shown);
  expect(shown_json["details"]["detail"].asString() == "disk I/O detail",
         "common HTTP error should include detail when enabled");
}

void test_base_path_normalization() {
  expect(karing::base_path::normalize("") == "/", "empty base path should normalize to root");
  expect(karing::base_path::normalize("/") == "/", "root base path should stay root");
  expect(karing::base_path::normalize("karing") == "/karing", "path should gain leading slash");
  expect(karing::base_path::normalize("/karing/") == "/karing", "path should lose trailing slash");
  expect(karing::base_path::normalize("https://example.test/karing/") == "/karing",
         "full URL should normalize to path");
  expect(karing::base_path::normalize("https://example.test/karing/?x=1#top") == "/karing",
         "full URL should drop query and fragment");
  expect(karing::base_path::normalize("https://example.test") == "/",
         "origin-only URL should normalize to root");

  expect(karing::base_path::matches("/", "/find"), "root base path should match any path");
  expect(karing::base_path::matches("/api", "/api"), "base path should match exact path");
  expect(karing::base_path::matches("/api", "/api/"), "base path should match trailing slash");
  expect(karing::base_path::matches("/api", "/api/find"), "base path should match nested path");
  expect(!karing::base_path::matches("/api", "/find"), "base path should reject root API path");
  expect(!karing::base_path::matches("/api", "/api2"), "base path should reject prefix collision");
  expect(!karing::base_path::matches("/api", "/apis"), "base path should reject plural prefix collision");
  expect(karing::base_path::strip("/api", "/api") == "/", "exact base path should strip to root");
  expect(karing::base_path::strip("/api", "/api/find") == "/find", "nested base path should strip prefix");
}

void test_listen_probe_rejects_used_port() {
#if defined(_WIN32)
  return;
#else
  const int fd = socket(AF_INET, SOCK_STREAM, 0);
  if (fd < 0) return;

  sockaddr_in addr{};
  addr.sin_family = AF_INET;
  addr.sin_addr.s_addr = htonl(INADDR_LOOPBACK);
  addr.sin_port = 0;
  if (bind(fd, reinterpret_cast<sockaddr*>(&addr), sizeof(addr)) != 0) {
    close(fd);
    throw test_failure("test listener socket should bind");
  }

  socklen_t len = sizeof(addr);
  if (getsockname(fd, reinterpret_cast<sockaddr*>(&addr), &len) != 0) {
    close(fd);
    throw test_failure("test listener socket should report port");
  }

  const int port = ntohs(addr.sin_port);
  const auto unavailable = karing::listen_probe::check_available("127.0.0.1", port);
  close(fd);

  expect(!unavailable.ok, "listen probe should reject an already bound port");
  expect(unavailable.error.find("127.0.0.1:" + std::to_string(port)) != std::string::npos,
         "listen probe error should include address and port");
#endif
}

void test_root_json_crud_and_delete() {
  const auto env = make_temp_env("root");
  expect(karing::db::init_sqlite_schema_file(env.db_path.string(), 4, false).ok, "db init should succeed");
  set_current_options(env);

  karing::controllers::karing_root_controller controller;

  Json::Value post_json(Json::objectValue);
  post_json["content"] = "hello root";
  auto post_req = make_json_request(drogon::Post, post_json);
  auto post_resp = invoke([&](auto&& cb) { controller.post_karing(post_req, std::move(cb)); });
  expect(post_resp->getStatusCode() == drogon::k201Created,
         "POST / should create, got status=" + std::to_string(static_cast<int>(post_resp->getStatusCode())) +
             " body=" + std::string(post_resp->getBody()));
  expect(response_json(post_resp)["id"].asInt() == 1, "created id should be 1");

  auto get_req = drogon::HttpRequest::newHttpRequest();
  get_req->setMethod(drogon::Get);
  auto get_resp = invoke([&](auto&& cb) { controller.get_karing(get_req, std::move(cb)); });
  expect(get_resp->getStatusCode() == drogon::k200OK, "GET / should succeed");
  expect(std::string(get_resp->getBody()) == "hello root", "GET / should return latest text");

  Json::Value put_json(Json::objectValue);
  put_json["content"] = "hello put";
  auto put_req = make_json_request(drogon::Put, put_json);
  put_req->setParameter("id", "1");
  auto put_resp = invoke([&](auto&& cb) { controller.put_karing(put_req, std::move(cb)); });
  expect(put_resp->getStatusCode() == drogon::k200OK, "PUT / should succeed");

  Json::Value patch_json(Json::objectValue);
  patch_json["content"] = "hello patch";
  auto patch_req = make_json_request(drogon::Patch, patch_json);
  patch_req->setParameter("id", "1");
  auto patch_resp = invoke([&](auto&& cb) { controller.patch_karing(patch_req, std::move(cb)); });
  expect(patch_resp->getStatusCode() == drogon::k200OK, "PATCH / should succeed");

  auto get_by_id = drogon::HttpRequest::newHttpRequest();
  get_by_id->setMethod(drogon::Get);
  get_by_id->setParameter("id", "1");
  auto get_by_id_resp = invoke([&](auto&& cb) { controller.get_karing(get_by_id, std::move(cb)); });
  expect(std::string(get_by_id_resp->getBody()) == "hello patch", "GET /?id=1 should reflect patch");

  auto delete_req = drogon::HttpRequest::newHttpRequest();
  delete_req->setMethod(drogon::Delete);
  auto delete_resp = invoke([&](auto&& cb) { controller.delete_karing(delete_req, std::move(cb)); });
  expect(delete_resp->getStatusCode() == drogon::k204NoContent, "DELETE / without id should delete latest recent record");

  auto second_delete_req = drogon::HttpRequest::newHttpRequest();
  second_delete_req->setMethod(drogon::Delete);
  auto second_delete_resp = invoke([&](auto&& cb) { controller.delete_karing(second_delete_req, std::move(cb)); });
  expect(second_delete_resp->getStatusCode() == drogon::k404NotFound, "DELETE / without id should not cascade");
}

void test_root_swap() {
  const auto env = make_temp_env("swap");
  expect(karing::db::init_sqlite_schema_file(env.db_path.string(), 4, false).ok, "db init should succeed");
  set_current_options(env);

  karing::dao::KaringDao dao(env.db_path.string(), env.upload_path.string());
  expect(dao.insert_text("swap-one") == 1, "insert first slot");
  expect(dao.insert_file("swap-two.txt", "text/plain", "swap-two") == 2, "insert second slot");

  karing::controllers::karing_root_controller controller;
  auto swap_req = drogon::HttpRequest::newHttpRequest();
  swap_req->setMethod(drogon::Post);
  swap_req->setParameter("id1", "1");
  swap_req->setParameter("id2", "2");
  auto swap_resp = invoke([&](auto&& cb) { controller.swap_karing(swap_req, std::move(cb)); });
  expect(swap_resp->getStatusCode() == drogon::k200OK, "POST /swap should succeed");
  auto swap_json = response_json(swap_resp);
  expect(swap_json["data"].isArray(), "swap response should return an array");
  expect(swap_json["data"].size() == 2, "swap response should return two records");
  expect(swap_json["data"][0]["id"].asInt() == 1, "swap response first record should be id 1");
  expect(swap_json["data"][0]["filename"].asString() == "swap-two.txt", "swap response should show swapped slot 1");
  expect(swap_json["data"][1]["id"].asInt() == 2, "swap response second record should be id 2");
  expect(swap_json["data"][1]["content"].asString() == "swap-one", "swap response should show swapped slot 2");

  auto first = dao.get_by_id(1);
  auto second = dao.get_by_id(2);
  expect(first.has_value(), "swapped slot 1 should exist");
  expect(second.has_value(), "swapped slot 2 should exist");
  expect(first->filename == "swap-two.txt", "slot 1 should now contain slot 2 record");
  expect(second->content == "swap-one", "slot 2 should now contain slot 1 record");

  auto same_swap_req = drogon::HttpRequest::newHttpRequest();
  same_swap_req->setMethod(drogon::Post);
  same_swap_req->setParameter("id1", "1");
  same_swap_req->setParameter("id2", "1");
  auto same_swap_resp = invoke([&](auto&& cb) { controller.swap_karing(same_swap_req, std::move(cb)); });
  expect(same_swap_resp->getStatusCode() == drogon::k400BadRequest, "POST /swap should reject same ids");
}

void test_root_move() {
  const auto env = make_temp_env("move");
  expect(karing::db::init_sqlite_schema_file(env.db_path.string(), 5, false).ok, "db init should succeed");
  set_current_options(env);

  karing::dao::KaringDao dao(env.db_path.string(), env.upload_path.string());
  expect(dao.insert_text("a") == 1, "insert slot 1");
  expect(dao.insert_text("b") == 2, "insert slot 2");
  expect(dao.insert_text("c") == 3, "insert slot 3");
  expect(dao.insert_text("d") == 4, "insert slot 4");
  expect(dao.insert_text("e") == 5, "insert slot 5");

  auto move_req = drogon::HttpRequest::newHttpRequest();
  move_req->setMethod(drogon::Post);
  move_req->setParameter("id", "5");
  move_req->setParameter("before", "2");
  karing::controllers::karing_root_controller controller;
  auto move_resp = invoke([&](auto&& cb) { controller.move_karing(move_req, std::move(cb)); });
  expect(move_resp->getStatusCode() == drogon::k200OK, "POST /move should succeed");

  auto json = response_json(move_resp);
  expect(json["data"].isArray(), "move response should return an array");
  expect(json["data"].size() == 5, "move response should return active records");
  expect(json["meta"]["next_id"].asInt() == 1, "move should return next_id");
  expect(dao.get_by_id(1)->content == "a", "slot 1 should remain a");
  expect(dao.get_by_id(2)->content == "e", "slot 2 should become e");
  expect(dao.get_by_id(3)->content == "b", "slot 3 should become b");
  expect(dao.get_by_id(4)->content == "c", "slot 4 should become c");
  expect(dao.get_by_id(5)->content == "d", "slot 5 should become d");

  auto same_move_req = drogon::HttpRequest::newHttpRequest();
  same_move_req->setMethod(drogon::Post);
  same_move_req->setParameter("id", "2");
  same_move_req->setParameter("before", "2");
  auto same_move_resp = invoke([&](auto&& cb) { controller.move_karing(same_move_req, std::move(cb)); });
  expect(same_move_resp->getStatusCode() == drogon::k400BadRequest, "POST /move should reject same ids");
}

void test_root_resequence() {
  const auto env = make_temp_env("resequence");
  expect(karing::db::init_sqlite_schema_file(env.db_path.string(), 5, false).ok, "db init should succeed");
  set_current_options(env);

  karing::dao::KaringDao dao(env.db_path.string(), env.upload_path.string());
  expect(dao.insert_text("one") == 1, "insert slot 1");
  expect(dao.insert_text("two") == 2, "insert slot 2");
  expect(dao.insert_text("three") == 3, "insert slot 3");
  expect(dao.insert_text("four") == 4, "insert slot 4");

  {
    karing::db::sqlite_connection db;
    db.open(env.db_path.string(), karing::db::sqlite_access::read_write);
    char* errmsg = nullptr;
    sqlite3_exec(db.get(),
                 "UPDATE entries SET used=0, source_kind=NULL, media_kind=NULL, content_text=NULL, file_path=NULL, "
                 "original_filename=NULL, mime_type=NULL, size_bytes=0, stored_at=NULL, updated_at=NULL WHERE id=2;"
                 "UPDATE store_state SET next_id=5 WHERE singleton_id=1;",
                 nullptr,
                 nullptr,
                 &errmsg);
    if (errmsg) sqlite3_free(errmsg);
  }

  auto resequence_req = drogon::HttpRequest::newHttpRequest();
  resequence_req->setMethod(drogon::Post);
  karing::controllers::karing_root_controller controller;
  auto resequence_resp = invoke([&](auto&& cb) { controller.resequence_karing(resequence_req, std::move(cb)); });
  expect(resequence_resp->getStatusCode() == drogon::k200OK, "POST /resequence should succeed");

  auto json = response_json(resequence_resp);
  expect(json["data"].isArray(), "resequence response should return an array");
  expect(json["data"].size() == 3, "resequence response should return active records");
  expect(json["data"][0]["id"].asInt() == 1, "resequence first record should be id 1");
  expect(json["meta"]["next_id"].asInt() == 4, "resequence should return next_id");

  auto first = dao.get_by_id(1);
  auto second = dao.get_by_id(2);
  auto third = dao.get_by_id(3);
  expect(first.has_value() && first->content == "one", "slot 1 should contain first oldest record");
  expect(second.has_value() && second->content == "three", "slot 2 should contain next record");
  expect(third.has_value() && third->content == "four", "slot 3 should contain last record");
}

void test_search_and_live_search() {
  const auto env = make_temp_env("search");
  expect(karing::db::init_sqlite_schema_file(env.db_path.string(), 5, false).ok, "db init should succeed");
  set_current_options(env);

  karing::dao::KaringDao dao(env.db_path.string(), env.upload_path.string());
  expect(dao.insert_text("alpha note") == 1, "insert alpha");
  expect(dao.insert_text("alpine note") == 2, "insert alpine");
  expect(dao.insert_file("audio.mp3", "audio/mpeg", "mp3") == 3, "insert file");

  karing::controllers::karing_search_controller search_controller;
  auto search_req = drogon::HttpRequest::newHttpRequest();
  search_req->setMethod(drogon::Get);
  search_req->setParameter("q", "alpha");
  search_req->setParameter("sort", "id");
  auto search_resp = invoke([&](auto&& cb) { search_controller.search(search_req, std::move(cb)); });
  expect(search_resp->getStatusCode() == drogon::k200OK, "GET /search should succeed");
  auto search_json = response_json(search_resp);
  expect(search_json["success"].asBool(), "/search should return success");
  expect(search_json["meta"]["sort"].asString() == "id", "/search should report sort");
  expect(search_json["data"].isArray(), "/search data should be an array");
  expect(search_json["data"].size() >= 1, "/search should return at least one result");

  auto exact_search_req = drogon::HttpRequest::newHttpRequest();
  exact_search_req->setMethod(drogon::Get);
  exact_search_req->setParameter("q", "alpine");
  auto exact_search_resp = invoke([&](auto&& cb) { search_controller.search(exact_search_req, std::move(cb)); });
  expect(exact_search_resp->getStatusCode() == drogon::k200OK, "GET /search exact term should succeed");
  expect(response_json(exact_search_resp)["data"].size() == 1, "/search should match full terms");

  auto prefix_search_req = drogon::HttpRequest::newHttpRequest();
  prefix_search_req->setMethod(drogon::Get);
  prefix_search_req->setParameter("q", "alp");
  auto prefix_search_resp = invoke([&](auto&& cb) { search_controller.search(prefix_search_req, std::move(cb)); });
  expect(prefix_search_resp->getStatusCode() == drogon::k200OK, "GET /search prefix-like term should succeed");
  expect(response_json(prefix_search_resp)["data"].size() == 0, "/search should not behave as prefix search");

  karing::controllers::karing_search_live_controller live_controller;
  auto live_req = drogon::HttpRequest::newHttpRequest();
  live_req->setMethod(drogon::Get);
  live_req->setParameter("q", "alp");
  live_req->setParameter("limit", "5");
  auto live_resp = invoke([&](auto&& cb) { live_controller.search_live(live_req, std::move(cb)); });
  expect(live_resp->getStatusCode() == drogon::k200OK, "GET /search/live should succeed");
  auto live_json = response_json(live_resp);
  expect(live_json["meta"]["live"].asBool(), "/search/live should mark live response");
  expect(live_json["data"].isArray(), "/search/live data should be an array");
  expect(live_json["data"].size() >= 2, "/search/live should return prefix matches");
  expect(live_json["data"][0].isMember("preview"), "/search/live text record should include preview");

  auto bad_search_req = drogon::HttpRequest::newHttpRequest();
  bad_search_req->setMethod(drogon::Get);
  bad_search_req->setParameter("sort", "bad");
  auto bad_search_resp = invoke([&](auto&& cb) { search_controller.search(bad_search_req, std::move(cb)); });
  expect(bad_search_resp->getStatusCode() == drogon::k400BadRequest, "/search should reject invalid sort");

  auto bad_live_req = drogon::HttpRequest::newHttpRequest();
  bad_live_req->setMethod(drogon::Get);
  auto bad_live_resp = invoke([&](auto&& cb) { live_controller.search_live(bad_live_req, std::move(cb)); });
  expect(bad_live_resp->getStatusCode() == drogon::k400BadRequest, "/search/live should require q");
}

void test_health_response() {
  const auto env = make_temp_env("health");
  expect(karing::db::init_sqlite_schema_file(env.db_path.string(), 3, false).ok, "db init should succeed");
  set_current_options(env);

  karing::controllers::health_controller controller;
  auto req = drogon::HttpRequest::newHttpRequest();
  req->setMethod(drogon::Get);
  auto resp = invoke([&](auto&& cb) { controller.health(req, std::move(cb)); });
  expect(resp->getStatusCode() == drogon::k200OK, "/health should succeed");
  auto json = response_json(resp);
  expect(json["status"].asString() == "ok", "health status should be ok");
  expect(json["limit"].asInt() == 25, "health should report runtime limit");
  expect(json["size"]["file"].asString() == "10MB", "health should report file size");
  expect(json["path"]["db"].asString() == env.db_path.string(), "health should report db path");
  expect(json["path"]["upload"].asString() == env.upload_path.string(), "health should report upload path");
  expect(json["path"]["log"].asString() == (env.root / "logs").string(), "health should report log path");
  expect(json["listener"]["address"].asString() == "127.0.0.1", "health should report listener address");
  expect(json["listener"]["port"].asInt() == 8080, "health should report listener port");
  expect(json["db"]["max_items"].asInt() == 3, "health should expose db max_items");
}

void test_upload_mime_support() {
  expect(karing::upload_mime::is_supported("application/json"), "application/json should be supported");
  expect(karing::upload_mime::is_supported("application/javascript"), "application/javascript should be supported");
  expect(karing::upload_mime::normalise("", "note.md") == "text/markdown", "markdown should infer text/markdown");
  expect(karing::upload_mime::normalise("", "data.json") == "application/json", "json should infer application/json");
  expect(karing::upload_mime::normalise("application/octet-stream", "main.cpp") == "text/plain", "cpp should infer text/plain");
  expect(karing::upload_mime::normalise("application/octet-stream", "index.ts") == "text/plain", "ts should infer text/plain");
}

void test_root_file_and_text_file_responses() {
  const auto env = make_temp_env("files");
  expect(karing::db::init_sqlite_schema_file(env.db_path.string(), 5, false).ok, "db init should succeed");
  set_current_options(env);

  karing::dao::KaringDao dao(env.db_path.string(), env.upload_path.string());
  expect(dao.insert_text("plain text") == 1, "insert plain text");
  expect(dao.insert_file("note.txt", "text/plain", "text file body") == 2, "insert text file");
  expect(dao.insert_file("sound.mp3", "audio/mpeg", "mp3-data") == 3, "insert audio file");

  karing::controllers::karing_root_controller controller;

  auto latest_req = drogon::HttpRequest::newHttpRequest();
  latest_req->setMethod(drogon::Get);
  auto latest_resp = invoke([&](auto&& cb) { controller.get_karing(latest_req, std::move(cb)); });
  expect(latest_resp->getStatusCode() == drogon::k200OK, "GET / latest should succeed");
  expect(latest_resp->contentTypeString().find("audio/mpeg") != std::string::npos, "latest file should preserve mime");
  expect(latest_resp->getHeader("content-disposition").find("inline") != std::string::npos,
         "latest file should default to inline");

  auto text_file_req = drogon::HttpRequest::newHttpRequest();
  text_file_req->setMethod(drogon::Get);
  text_file_req->setParameter("id", "2");
  auto text_file_resp = invoke([&](auto&& cb) { controller.get_karing(text_file_req, std::move(cb)); });
  expect(text_file_resp->getStatusCode() == drogon::k200OK, "GET /?id=text-file should succeed");
  expect(std::string(text_file_resp->getBody()) == "text file body", "text file should be readable as text");
  expect(text_file_resp->contentTypeString().find("text/plain") != std::string::npos,
         "text file should return text/plain content type");

  auto text_file_download_req = drogon::HttpRequest::newHttpRequest();
  text_file_download_req->setMethod(drogon::Get);
  text_file_download_req->setParameter("id", "2");
  text_file_download_req->setParameter("as", "download");
  auto text_file_download_resp =
      invoke([&](auto&& cb) { controller.get_karing(text_file_download_req, std::move(cb)); });
  expect(text_file_download_resp->getStatusCode() == drogon::k200OK, "GET text-file as download should succeed");
  expect(text_file_download_resp->getHeader("content-disposition").find("attachment") != std::string::npos,
         "text file download should use attachment");
  expect(text_file_download_resp->getHeader("content-disposition").find("note.txt") != std::string::npos,
         "text file download should preserve filename");

  auto audio_download_req = drogon::HttpRequest::newHttpRequest();
  audio_download_req->setMethod(drogon::Get);
  audio_download_req->setParameter("id", "3");
  audio_download_req->setParameter("as", "download");
  auto audio_download_resp = invoke([&](auto&& cb) { controller.get_karing(audio_download_req, std::move(cb)); });
  expect(audio_download_resp->getStatusCode() == drogon::k200OK, "GET audio as download should succeed");
  expect(audio_download_resp->getHeader("content-disposition").find("attachment") != std::string::npos,
         "audio download should use attachment");
  expect(audio_download_resp->getHeader("content-disposition").find("sound.mp3") != std::string::npos,
         "audio download should preserve filename");

  expect(dao.insert_file("日本語ファイル名.mp3", "audio/mpeg", "jp-mp3") == 4, "insert unicode audio file");
  auto unicode_download_req = drogon::HttpRequest::newHttpRequest();
  unicode_download_req->setMethod(drogon::Get);
  unicode_download_req->setParameter("id", "4");
  unicode_download_req->setParameter("as", "download");
  auto unicode_download_resp =
      invoke([&](auto&& cb) { controller.get_karing(unicode_download_req, std::move(cb)); });
  expect(unicode_download_resp->getStatusCode() == drogon::k200OK, "GET unicode file as download should succeed");
  const auto unicode_disposition = unicode_download_resp->getHeader("content-disposition");
  expect(unicode_disposition.find("attachment") != std::string::npos,
         "unicode file download should use attachment");
  expect(unicode_disposition.find("filename*=") != std::string::npos,
         "unicode file download should include filename*");
  expect(unicode_disposition.find("UTF-8''") != std::string::npos,
         "unicode file download should use RFC 5987 encoding");
}

}  // namespace

int main() {
  const std::vector<std::pair<std::string, std::function<void()>>> tests = {
      {"options_parse_modes", test_options_parse_modes},
      {"common_http_error_response_hides_and_shows_detail", test_common_http_error_response_hides_and_shows_detail},
      {"base_path_normalization", test_base_path_normalization},
      {"listen_probe_rejects_used_port", test_listen_probe_rejects_used_port},
      {"root_json_crud_and_delete", test_root_json_crud_and_delete},
      {"root_swap", test_root_swap},
      {"root_move", test_root_move},
      {"root_resequence", test_root_resequence},
      {"root_file_and_text_file_responses", test_root_file_and_text_file_responses},
      {"search_and_live_search", test_search_and_live_search},
      {"health_response", test_health_response},
      {"upload_mime_support", test_upload_mime_support},
  };

  int failed = 0;
  for (const auto& [name, test] : tests) {
    try {
      test();
      std::cout << "[PASS] " << name << "\n";
    } catch (const std::exception& ex) {
      ++failed;
      std::cerr << "[FAIL] " << name << ": " << ex.what() << "\n";
    }
  }
  return failed == 0 ? 0 : 1;
}

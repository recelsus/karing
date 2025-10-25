// Basic CLI smoke tests for karing binary
// These tests exercise CLI subcommands and ensure no crashes and expected formats.

#include <catch2/catch_test_macros.hpp>
#include <string>
#include <array>
#include <cstdio>
#include <memory>
#include <stdexcept>
#include <filesystem>
#include <sstream>
#include <chrono>
#include <thread>
#include <unistd.h>

namespace fs = std::filesystem;

static std::string run_read_stdout(const std::string& cmd, int& exit_code) {
  // Run command via popen and capture stdout; return output and set exit_code.
  std::array<char, 4096> buffer{};
  std::string result;
  FILE* pipe = popen(cmd.c_str(), "r");
  if (!pipe) {
    exit_code = -1;
    return result;
  }
  while (fgets(buffer.data(), buffer.size(), pipe)) {
    result.append(buffer.data());
  }
  int rc = pclose(pipe);
  // WEXITSTATUS is POSIX; rc contains encoded status
  if (WIFEXITED(rc)) exit_code = WEXITSTATUS(rc); else exit_code = rc;
  return result;
}

static std::string last_non_empty_line(std::string s) {
  // Return the last non-empty line from s (trims CR/LF)
  if (s.empty()) return s;
  for (auto& ch : s) if (ch=='\r') ch='\n';
  while (!s.empty() && s.back()=='\n') s.pop_back();
  auto pos = s.find_last_of('\n');
  std::string line = (pos==std::string::npos)? s : s.substr(pos+1);
  while (!line.empty() && (line.back()==' ')) line.pop_back();
  while (!line.empty() && (line.front()==' ')) line.erase(line.begin());
  return line;
}

static std::string make_temp_db_path(const std::string& prefix) {
  // Create unique temp subdir in build tree and return db path under it.
  fs::path base = fs::current_path() / (prefix + "_work");
  fs::create_directories(base);
  std::ostringstream oss;
  oss << std::hex << std::this_thread::get_id();
  std::string tid = oss.str();
  fs::path db = base / ("karing_" + tid + ".db");
  return db.string();
}

TEST_CASE("keys add prints 48-hex", "[cli]") {
  std::string db = make_temp_db_path("cli1");
  int code = 0;
  std::string out = run_read_stdout(std::string("./karing keys add --data ") + db, code);
  REQUIRE(code == 0);
  std::string key = last_non_empty_line(out);
  REQUIRE(key.size() == 48);
  for (char c : key) {
    bool is_hex = (('0' <= c && c <= '9') || ('a' <= c && c <= 'f'));
    REQUIRE(is_hex);
  }
}

TEST_CASE("ip add normalises CIDR", "[cli]") {
  std::string db = make_temp_db_path("cli2");
  int code = 0;
  // Add raw host 10.0.0.5/24 and check it appears as 10.0.0.0/24
  std::string out1 = run_read_stdout(std::string("./karing ip add 10.0.0.5/24 allow --data ") + db, code);
  REQUIRE(code == 0);
  std::string out2 = run_read_stdout(std::string("./karing ip list allow --data ") + db, code);
  REQUIRE(code == 0);
  REQUIRE(out2.find("ip_allow") != std::string::npos);
  REQUIRE(out2.find("id\tcidr\tenabled\tcreated_at") != std::string::npos);
  REQUIRE(out2.find("10.0.0.0/24") != std::string::npos);
}

TEST_CASE("keys list handles empty label", "[cli]") {
  std::string db = make_temp_db_path("cli3");
  int code = 0;
  std::string json = run_read_stdout(std::string("./karing keys add --json --data ") + db, code);
  REQUIRE(code == 0);
  std::string line = last_non_empty_line(json);
  auto id_pos = line.find("\"id\":");
  REQUIRE(id_pos != std::string::npos);
  auto id_end = line.find_first_of(",}", id_pos + 5);
  REQUIRE(id_end != std::string::npos);
  int id = std::stoi(line.substr(id_pos + 5, id_end - (id_pos + 5)));
  auto secret_pos = line.find("\"secret\":\"");
  REQUIRE(secret_pos != std::string::npos);
  auto secret_end = line.find('"', secret_pos + 11);
  REQUIRE(secret_end != std::string::npos);
  std::string key = line.substr(secret_pos + 11, secret_end - (secret_pos + 11));

  std::string disable = run_read_stdout(std::string("./karing keys disable ") + std::to_string(id) + " --data " + db, code);
  REQUIRE(code == 0);
  REQUIRE(disable.find("disabled") != std::string::npos);

  std::string out = run_read_stdout(std::string("./karing keys list --data ") + db, code);
  REQUIRE(code == 0);
  REQUIRE(out.find("id\tkey\tlabel\trole\tenabled\tcreated_at\tlast_used_at\tlast_ip") != std::string::npos);
  REQUIRE(out.find(key) != std::string::npos);
}

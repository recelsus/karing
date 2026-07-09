#include <chrono>
#include <filesystem>
#include <fstream>
#include <functional>
#include <iostream>
#include <stdexcept>
#include <string>
#include <vector>

#include "utils/arg_utils.h"
#include "utils/mime.h"

namespace fs = std::filesystem;

namespace {

struct test_failure : std::runtime_error {
  using std::runtime_error::runtime_error;
};

void expect(bool condition, const std::string& message) {
  if (!condition) throw test_failure(message);
}

fs::path make_temp_root(const std::string& name) {
  const auto now = std::chrono::steady_clock::now().time_since_epoch().count();
  const auto root = fs::temp_directory_path() / ("karing-cli-test-" + name + "-" + std::to_string(now));
  fs::create_directories(root);
  return root;
}

void write_file(const fs::path& path, const std::string& data) {
  std::ofstream out(path, std::ios::binary | std::ios::trunc);
  if (!out.is_open()) throw test_failure("failed to create fixture");
  out.write(data.data(), static_cast<std::streamsize>(data.size()));
}

void test_arg_utils() {
  expect(karing::cli::utils::join_words({"hello", "world"}) == "hello world", "join_words should join with spaces");
  expect(karing::cli::utils::is_valid_id("1"), "id 1 should be valid");
  expect(karing::cli::utils::is_valid_id("1000"), "id 1000 should be valid");
  expect(!karing::cli::utils::is_valid_id("0"), "id 0 should be invalid");
  expect(!karing::cli::utils::is_valid_id("1001"), "id 1001 should be invalid");
  expect(!karing::cli::utils::is_valid_id("text"), "non-numeric id should be invalid");
}

void test_mime_guessing() {
  const auto root = make_temp_root("mime");
  const auto markdown = root / "note.md";
  const auto source = root / "main.cpp";
  const auto extensionless_text = root / ".bashrc";
  const auto binary = root / "blob.unknown";

  write_file(markdown, "# note\n");
  write_file(source, "int main() { return 0; }\n");
  write_file(extensionless_text, "alias ll='ls -la'\n");
  write_file(binary, std::string("abc\0def", 7));

  expect(karing::cli::utils::guess_mime_type(markdown).value_or("") == "text/markdown", "markdown mime should be inferred");
  expect(karing::cli::utils::guess_mime_type(source).value_or("") == "text/plain", "source mime should be inferred as text/plain");
  expect(karing::cli::utils::guess_mime_type(extensionless_text).value_or("") == "text/plain", "text-like extensionless files should be text/plain");
  expect(!karing::cli::utils::guess_mime_type(binary).has_value(), "unknown binary files should not get a text mime");
}

}  // namespace

int main() {
  const std::vector<std::pair<std::string, std::function<void()>>> tests = {
      {"arg_utils", test_arg_utils},
      {"mime_guessing", test_mime_guessing},
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

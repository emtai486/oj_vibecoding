#include "config/config.h"
#include "db/mysql_client.h"

#include <httplib.h>

#include <filesystem>
#include <fstream>
#include <iostream>
#include <sstream>
#include <stdexcept>
#include <string>
#include <vector>

namespace {

namespace fs = std::filesystem;

int failures = 0;

void expect(bool condition, const std::string& message) {
  if (!condition) {
    std::cerr << "FAIL: " << message << '\n';
    ++failures;
  }
}

template <typename T>
std::string value_to_string(const T& value) {
  std::ostringstream output;
  output << value;
  return output.str();
}

template <typename T>
void expect_equal(const T& actual, const T& expected,
                  const std::string& message) {
  if (!(actual == expected)) {
    expect(false, message + " (expected " + value_to_string(expected) +
                      ", got " + value_to_string(actual) + ")");
  }
}

std::string read_text(const fs::path& path) {
  std::ifstream input(path);
  if (!input) {
    throw std::runtime_error("failed to read " + path.string());
  }

  std::ostringstream output;
  output << input.rdbuf();
  return output.str();
}

void write_text(const fs::path& path, const std::string& content) {
  fs::create_directories(path.parent_path());

  std::ofstream output(path);
  if (!output) {
    throw std::runtime_error("failed to write " + path.string());
  }

  output << content;
}

void expect_dir(const fs::path& root, const std::string& relative_path) {
  const fs::path path = root / relative_path;
  expect(fs::is_directory(path), relative_path + " should be a directory");
}

void expect_file(const fs::path& root, const std::string& relative_path) {
  const fs::path path = root / relative_path;
  expect(fs::is_regular_file(path), relative_path + " should be a file");
}

void expect_non_empty_file(const fs::path& root,
                           const std::string& relative_path) {
  const fs::path path = root / relative_path;
  expect_file(root, relative_path);
  if (fs::is_regular_file(path)) {
    expect(fs::file_size(path) > 0, relative_path + " should not be empty");
  }
}

void expect_contains(const std::string& text, const std::string& needle,
                     const std::string& message) {
  expect(text.find(needle) != std::string::npos, message);
}

void test_project_structure(const fs::path& root) {
  const std::vector<std::string> required_dirs = {
      "config",
      "scripts",
      "sql",
      "third_party",
      "third_party/httplib",
      "third_party/json",
      "src",
      "src/app",
      "src/api",
      "src/auth",
      "src/config",
      "src/db",
      "src/judge",
      "src/model",
      "src/util",
      "public",
      "public/admin",
      "public/css",
      "public/js",
      "public/vendor",
      "public/vendor/codemirror",
      "var",
      "var/judge_tmp",
      "tests",
      "tests/api",
      "tests/db",
      "tests/judge",
  };

  for (const auto& relative_path : required_dirs) {
    expect_dir(root, relative_path);
  }

  expect_non_empty_file(root, "SPEC.md");
  expect_non_empty_file(root, "README.md");
  expect_non_empty_file(root, "Makefile");
  expect_non_empty_file(root, "config/app.example.conf");
}

void test_cpp_httplib_dependency(const fs::path& root) {
  expect_non_empty_file(root, "third_party/httplib/httplib.h");

  httplib::Server server;
  (void)server.Get("/unit-health",
                   [](const httplib::Request&, httplib::Response& response) {
                     response.set_content("ok", "text/plain");
                   });
}

void test_mysql_config_and_client(const fs::path& root) {
  const auto config =
      oj::config::load_config((root / "config/app.example.conf").string());

  expect_equal(config.server.host, std::string("0.0.0.0"),
               "server.host should load from example config");
  expect_equal(config.server.port, static_cast<std::uint16_t>(8080),
               "server.port should load from example config");
  expect_equal(config.mysql.host, std::string("127.0.0.1"),
               "mysql.host should load from example config");
  expect_equal(config.mysql.port, static_cast<std::uint16_t>(3306),
               "mysql.port should load from example config");
  expect_equal(config.mysql.user, std::string("oj_user"),
               "mysql.user should load from example config");
  expect_equal(config.mysql.password, std::string("change_me"),
               "mysql.password should load from example config");
  expect_equal(config.mysql.database, std::string("oj"),
               "mysql.database should load from example config");
  expect_equal(config.mysql.charset, std::string("utf8mb4"),
               "mysql.charset should load from example config");
  expect_equal(config.mysql.connect_timeout_seconds,
               static_cast<std::uint32_t>(5),
               "mysql.connect_timeout_seconds should load from example config");

  const fs::path temp_config = root / "build/tests/tmp/custom.conf";
  write_text(temp_config,
             "# custom values\n"
             "server.host=127.0.0.1\n"
             "server.port=9090\n"
             "mysql.host=db.local\n"
             "mysql.port=3307\n"
             "mysql.user=test_user\n"
             "mysql.password=test_password\n"
             "mysql.database=test_db\n"
             "mysql.charset=utf8mb4\n"
             "mysql.connect_timeout_seconds=9\n");

  const auto custom_config = oj::config::load_config(temp_config.string());
  expect_equal(custom_config.server.host, std::string("127.0.0.1"),
               "custom server.host should load");
  expect_equal(custom_config.server.port, static_cast<std::uint16_t>(9090),
               "custom server.port should load");
  expect_equal(custom_config.mysql.host, std::string("db.local"),
               "custom mysql.host should load");
  expect_equal(custom_config.mysql.port, static_cast<std::uint16_t>(3307),
               "custom mysql.port should load");
  expect_equal(custom_config.mysql.connect_timeout_seconds,
               static_cast<std::uint32_t>(9),
               "custom mysql timeout should load");

  const fs::path invalid_config = root / "build/tests/tmp/invalid.conf";
  write_text(invalid_config, "server.port=70000\n");

  bool invalid_port_failed = false;
  try {
    (void)oj::config::load_config(invalid_config.string());
  } catch (const std::exception&) {
    invalid_port_failed = true;
  }
  expect(invalid_port_failed, "invalid server.port should be rejected");

  oj::db::MySqlClient client(config.mysql);
  std::string error;
  expect(!client.ping(&error), "ping before connect should fail");
  expect_equal(error, std::string("mysql connection is not open"),
               "ping before connect should report a clear error");
  expect(client.raw() == nullptr, "raw mysql handle should be null before connect");
}

void test_static_assets(const fs::path& root) {
  const std::vector<std::string> static_files = {
      "public/index.html",
      "public/problem.html",
      "public/login.html",
      "public/register.html",
      "public/admin/login.html",
      "public/admin/index.html",
      "public/admin/new-problem.html",
      "public/css/base.css",
      "public/css/layout.css",
      "public/css/admin.css",
      "public/js/api.js",
      "public/js/auth.js",
      "public/js/problem-list.js",
      "public/js/problem-detail.js",
      "public/js/editor.js",
      "public/js/storage.js",
      "public/js/admin.js",
  };

  for (const auto& relative_path : static_files) {
    expect_non_empty_file(root, relative_path);
  }
}

void test_local_editor_dependency(const fs::path& root) {
  const std::vector<std::string> codemirror_files = {
      "public/vendor/codemirror/codemirror.js",
      "public/vendor/codemirror/codemirror.css",
      "public/vendor/codemirror/theme/material-darker.css",
      "public/vendor/codemirror/mode/clike/clike.js",
  };

  for (const auto& relative_path : codemirror_files) {
    expect_non_empty_file(root, relative_path);
  }

  expect_contains(read_text(root / "public/vendor/codemirror/codemirror.js"),
                  "CodeMirror", "local CodeMirror script should be present");
  expect_contains(read_text(root / "public/vendor/codemirror/mode/clike/clike.js"),
                  "text/x-c++src",
                  "local CodeMirror C/C++ mode should be present");
}

void test_readme(const fs::path& root) {
  const std::string readme = read_text(root / "README.md");

  expect_contains(readme, "cpp-httplib", "README should document cpp-httplib");
  expect_contains(readme, "MySQL", "README should document MySQL");
  expect_contains(readme, "CodeMirror", "README should document CodeMirror");
  expect_contains(readme, "make", "README should document build command");
  expect_contains(readme, "--check-db",
                  "README should document MySQL connectivity check");
}

}  // namespace

int main(int argc, char* argv[]) {
  const fs::path root =
      argc >= 2 ? fs::absolute(argv[1]) : fs::current_path();

  try {
    test_project_structure(root);
    test_cpp_httplib_dependency(root);
    test_mysql_config_and_client(root);
    test_static_assets(root);
    test_local_editor_dependency(root);
    test_readme(root);
  } catch (const std::exception& error) {
    std::cerr << "FAIL: unexpected exception: " << error.what() << '\n';
    return 1;
  }

  if (failures != 0) {
    std::cerr << failures << " initialization test(s) failed\n";
    return 1;
  }

  std::cout << "PASS: SPEC 12.1 initialization checks passed\n";
  return 0;
}

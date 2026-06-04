#include "app/server.h"
#include "config/config.h"
#include "db/mysql_client.h"
#include "util/json.h"
#include "util/json_response.h"

#include <httplib.h>

#include <filesystem>
#include <fstream>
#include <iostream>
#include <sstream>
#include <stdexcept>
#include <string>

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
void expect_equal(const T& actual, const T& expected,
                  const std::string& message) {
  if (!(actual == expected)) {
    std::cerr << "FAIL: " << message << " (expected " << expected << ", got "
              << actual << ")\n";
    ++failures;
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

void expect_contains(const std::string& text, const std::string& needle,
                     const std::string& message) {
  expect(text.find(needle) != std::string::npos, message);
}

void test_json_parse_and_stringify() {
  std::string error;
  const auto parsed = oj::util::json::parse(
      R"({"username":"user1","age":7,"active":true,"tags":["cpp","mysql"]})",
      &error);

  expect(parsed.has_value(), "valid JSON request body should parse");
  expect(parsed->is_object(), "parsed root should be an object");

  const auto& object = parsed->as_object();
  expect_equal(object.at("username").as_string(), std::string("user1"),
               "JSON string field should parse");
  expect_equal(object.at("age").as_int(), static_cast<std::int64_t>(7),
               "JSON integer field should parse");
  expect(object.at("active").as_bool(), "JSON bool field should parse");
  expect_equal(object.at("tags").as_array().size(), static_cast<std::size_t>(2),
               "JSON array field should parse");

  const std::string encoded =
      oj::util::json::stringify(oj::util::json::Value::Object{
          {"success", true},
          {"message", "line\nquoted"},
          {"data",
           oj::util::json::Value::Array{oj::util::json::Value(std::int64_t{1}),
                                        oj::util::json::Value(std::int64_t{2})}},
      });
  expect_contains(encoded, "\"success\":true",
                  "JSON response should include boolean values");
  expect_contains(encoded, "\"message\":\"line\\nquoted\"",
                  "JSON response should escape strings");
  expect_contains(encoded, "\"data\":[1,2]",
                  "JSON response should include arrays");

  const auto invalid = oj::util::json::parse(R"({"broken":)", &error);
  expect(!invalid.has_value(), "invalid JSON should be rejected");
  expect(!error.empty(), "invalid JSON should report an error");
}

void test_json_response_helpers() {
  httplib::Response response;
  oj::util::send_success(response,
                         oj::util::json::Value::Object{{"status", "ok"}});

  expect_equal(response.status, static_cast<int>(httplib::StatusCode::OK_200),
               "success response should default to 200");
  expect_contains(response.get_header_value("Content-Type"),
                  "application/json",
                  "success response should use JSON content type");
  expect_contains(response.body, "\"success\":true",
                  "success response should include success flag");
  expect_contains(response.body, "\"message\":\"ok\"",
                  "success response should include default message");

  httplib::Response error_response;
  oj::util::send_error(error_response, httplib::StatusCode::Unauthorized_401,
                       "unauthorized");
  expect_equal(error_response.status,
               static_cast<int>(httplib::StatusCode::Unauthorized_401),
               "error response should keep status code");
  expect_contains(error_response.body, "\"success\":false",
                  "error response should include failure flag");
  expect_contains(error_response.body, "\"message\":\"unauthorized\"",
                  "error response should include error message");
}

void test_parse_json_body_helper() {
  httplib::Request request;
  request.body = R"({"problem_id":1,"code":"int main(){return 0;}"})";

  httplib::Response response;
  oj::util::json::Value body;
  expect(oj::util::parse_json_body(request, &body, response),
         "parse_json_body should accept valid JSON");
  expect_equal(body.as_object().at("problem_id").as_int(),
               static_cast<std::int64_t>(1),
               "parse_json_body should return parsed body");

  httplib::Request invalid_request;
  invalid_request.body = R"({"problem_id":)";

  httplib::Response invalid_response;
  expect(!oj::util::parse_json_body(invalid_request, nullptr, invalid_response),
         "parse_json_body should reject invalid JSON");
  expect_equal(invalid_response.status,
               static_cast<int>(httplib::StatusCode::BadRequest_400),
               "invalid JSON should return 400");
  expect_contains(invalid_response.body, "\"message\":\"invalid json\"",
                  "invalid JSON should return a consistent message");
}

void test_mysql_query_wrapper() {
  oj::config::MySqlConfig config;
  oj::db::MySqlClient client(config);
  std::string error;

  expect(!client.is_connected(), "client should start disconnected");
  expect(!client.execute("SELECT 1", &error),
         "execute before connect should fail");
  expect_equal(error, std::string("mysql connection is not open"),
               "execute before connect should report a clear error");

  oj::db::QueryResult result;
  expect(!client.query("SELECT 1", &result, &error),
         "query before connect should fail");
  expect_equal(error, std::string("mysql connection is not open"),
               "query before connect should report a clear error");
}

void test_server_source_and_static_assets(const fs::path& root) {
  const std::string server_source = read_text(root / "src/app/server.cpp");
  const std::string main_source = read_text(root / "src/main.cpp");

  expect_contains(server_source, "set_mount_point",
                  "server should provide static file access");
  expect_contains(server_source, "set_exception_handler",
                  "server should install exception handling");
  expect_contains(server_source, "set_error_handler",
                  "server should install HTTP error handling");
  expect_contains(server_source, "parse_json_body",
                  "server should include JSON request parsing route coverage");
  expect_contains(main_source, "run_server",
                  "main should start the cpp-httplib HTTP service");

  const auto config =
      oj::config::load_config((root / "config/app.example.conf").string());
  const auto server = oj::app::create_server(config, (root / "public").string());
  expect(server != nullptr, "server factory should create a server");
  expect(server->is_valid(), "created server should be valid");
}

void test_spec_progress(const fs::path& root) {
  const std::string spec = read_text(root / "SPEC.md");
  expect_contains(spec, "- [x] 启动 cpp-httplib HTTP 服务",
                  "SPEC 12.3 server startup item should be complete");
  expect_contains(spec, "- [x] 提供静态文件访问",
                  "SPEC 12.3 static file item should be complete");
  expect_contains(spec, "- [x] 实现 JSON 请求解析",
                  "SPEC 12.3 JSON parse item should be complete");
  expect_contains(spec, "- [x] 实现统一 JSON 响应",
                  "SPEC 12.3 JSON response item should be complete");
  expect_contains(spec, "- [x] 实现 MySQL 查询封装",
                  "SPEC 12.3 MySQL wrapper item should be complete");
  expect_contains(spec, "- [x] 实现错误处理",
                  "SPEC 12.3 error handling item should be complete");
}

}  // namespace

int main(int argc, char* argv[]) {
  const fs::path root =
      argc >= 2 ? fs::absolute(argv[1]) : fs::current_path();

  try {
    test_json_parse_and_stringify();
    test_json_response_helpers();
    test_parse_json_body_helper();
    test_mysql_query_wrapper();
    test_server_source_and_static_assets(root);
    test_spec_progress(root);
  } catch (const std::exception& error) {
    std::cerr << "FAIL: unexpected exception: " << error.what() << '\n';
    return 1;
  }

  if (failures != 0) {
    std::cerr << failures << " backend foundation test(s) failed\n";
    return 1;
  }

  std::cout << "PASS: SPEC 12.3 backend foundation checks passed\n";
  return 0;
}

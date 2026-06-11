#include "auth/password.h"
#include "auth/session.h"
#include "judge/comparator.h"

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

void test_seed_password_hashes() {
  expect(oj::auth::verify_password(
             "password",
             "pbkdf2_sha256$120000$oj-user1-v1$"
             "559a3423ecafda16f42a4b30389959d58e637b750cbb65eb77edb31333bdf2c4"),
         "seeded user password should verify");
  expect(!oj::auth::verify_password(
             "wrong",
             "pbkdf2_sha256$120000$oj-user1-v1$"
             "559a3423ecafda16f42a4b30389959d58e637b750cbb65eb77edb31333bdf2c4"),
         "wrong password should not verify");

  const std::string generated = oj::auth::hash_password("new-password");
  expect(oj::auth::verify_password("new-password", generated),
         "generated registration password hash should verify");
}

void test_user_session_cookie() {
  oj::auth::SessionStore sessions;
  const std::string session_id = sessions.create_user_session(7, "user1");
  const auto user = sessions.find_user_session(session_id);

  expect(user.has_value(), "created session should be retrievable");
  expect_equal(user->id, static_cast<std::uint64_t>(7),
               "session should keep user id");
  expect_equal(user->username, std::string("user1"),
               "session should keep username");

  httplib::Request request;
  request.set_header("Cookie", "theme=light; oj_user_session=" + session_id);
  const auto cookie = oj::auth::cookie_value(request, "oj_user_session");
  expect(cookie.has_value(), "session cookie should be parsed from Cookie header");
  expect_equal(*cookie, session_id, "parsed session cookie should match");

  httplib::Response response;
  oj::auth::set_user_session_cookie(response, session_id);
  expect_contains(response.get_header_value("Set-Cookie"), "oj_user_session=",
                  "login should set user session cookie");
  expect_contains(response.get_header_value("Set-Cookie"), "HttpOnly",
                  "session cookie should be HttpOnly");

  sessions.destroy_user_session(session_id);
  expect(!sessions.find_user_session(session_id).has_value(),
         "destroyed session should not be retrievable");

  const std::string admin_session_id =
      sessions.create_admin_session(3, "admin");
  const auto admin = sessions.find_admin_session(admin_session_id);
  expect(admin.has_value(), "created admin session should be retrievable");
  expect_equal(admin->username, std::string("admin"),
               "admin session should keep username");

  httplib::Response admin_response;
  oj::auth::set_admin_session_cookie(admin_response, admin_session_id);
  expect_contains(admin_response.get_header_value("Set-Cookie"),
                  "oj_admin_session=",
                  "admin login should set admin session cookie");
  expect_contains(admin_response.get_header_value("Set-Cookie"), "HttpOnly",
                  "admin session cookie should be HttpOnly");

  sessions.destroy_admin_session(admin_session_id);
  expect(!sessions.find_admin_session(admin_session_id).has_value(),
         "destroyed admin session should not be retrievable");
}

void test_output_comparator() {
  expect(oj::judge::compare_output("3\n", "3\n", "strict"),
         "strict mode should accept exact output");
  expect(!oj::judge::compare_output("3", "3\n", "strict"),
         "strict mode should reject missing newline");
  expect(oj::judge::compare_output("2.34\n", "2.3\n", "float_1"),
         "float_1 mode should compare rounded numeric tokens");
  expect(!oj::judge::compare_output("2.36\n", "2.3\n", "float_1"),
         "float_1 mode should reject different one-decimal values");
  expect(!oj::judge::compare_output("ok 1\n", "ok 1 2\n", "float_1"),
         "float_1 mode should reject token count mismatch");
}

void test_user_feature_sources(const fs::path& root) {
  const std::string server = read_text(root / "src/app/server.cpp");
  const std::string submit_api = read_text(root / "src/api/submit_api.cpp");
  const std::string admin_api = read_text(root / "src/api/admin_api.cpp");
  const std::string admin_repository =
      read_text(root / "src/db/admin_repository.cpp");
  const std::string problem_repository =
      read_text(root / "src/db/problem_repository.cpp");
  const std::string judge_service =
      read_text(root / "src/judge/judge_service.cpp");
  const std::string comparator =
      read_text(root / "src/judge/comparator.cpp");
  const std::string user_api = read_text(root / "src/api/user_api.cpp");
  const std::string problem_api = read_text(root / "src/api/problem_api.cpp");
  const std::string storage = read_text(root / "public/js/storage.js");
  const std::string admin_js = read_text(root / "public/js/admin.js");
  const std::string problem_detail =
      read_text(root / "public/js/problem-detail.js");
  const std::string problem_list =
      read_text(root / "public/js/problem-list.js");

  expect_contains(server, "register_problem_routes",
                  "server should register problem API routes");
  expect_contains(server, "register_user_routes",
                  "server should register user API routes");
  expect_contains(server, "register_submit_routes",
                  "server should register submit API route");
  expect_contains(server, "register_admin_routes",
                  "server should register admin API routes");
  expect_contains(problem_api, "/api/problems",
                  "problem API should expose problem list");
  expect_contains(problem_api, R"(/api/problems/(\d+))",
                  "problem API should expose problem detail route");
  expect_contains(user_api, "/api/user/login",
                  "user API should expose login");
  expect_contains(user_api, "/api/user/logout",
                  "user API should expose logout");
  expect_contains(submit_api, "unauthorized",
                  "submit API should reject anonymous users");
  expect_contains(submit_api, "has_submit_session",
                  "submit API should accept authenticated submitters");
  expect_contains(submit_api, "JudgeService",
                  "submit API should call judge service");
  expect_contains(admin_repository, "FROM admins WHERE username",
                  "admin repository should load seeded admins");
  expect_contains(admin_api, "/api/admin/login",
                  "admin API should expose login");
  expect_contains(admin_api, "/api/admin/logout",
                  "admin API should expose logout");
  expect_contains(admin_api, "/api/admin/problems",
                  "admin API should expose problem creation");
  expect_contains(admin_api, "oj_admin_session",
                  "admin API should use an admin session cookie");
  expect_contains(admin_api, "delete_by_id",
                  "admin API should delete problems");
  expect_contains(problem_repository, "INSERT INTO testcases",
                  "admin problem creation should persist testcases");
  expect_contains(problem_repository, "DELETE FROM problems WHERE id",
                  "problem repository should physically delete problems");
  expect_contains(judge_service, "TempDirectory",
                  "judge service should create a temporary directory");
  expect_contains(judge_service, "main.cpp",
                  "judge service should write user code to a C++ source file");
  expect_contains(judge_service, R"({"g++", "main.cpp")",
                  "judge service should compile with g++");
  expect_contains(judge_service, "compile_code(temp.path())",
                  "judge service should capture compile success or failure");
  expect_contains(judge_service, "WrongAnswer",
                  "judge service should distinguish wrong answers");
  expect_contains(judge_service, "CompileError",
                  "judge service should distinguish compile errors");
  expect_contains(judge_service, "TimeLimitExceeded",
                  "judge service should distinguish time limit exceeded");
  expect_contains(judge_service, "MemoryLimitExceeded",
                  "judge service should distinguish memory limit exceeded");
  expect_contains(judge_service, "OutputLimitExceeded",
                  "judge service should distinguish output limit exceeded");
  expect_contains(judge_service, R"({"./main"})",
                  "judge service should execute the compiled program");
  expect_contains(judge_service, "stdin_path",
                  "judge service should pass testcase input to stdin");
  expect_contains(judge_service, "stdout_path",
                  "judge service should capture stdout");
  expect_contains(judge_service, "timeout_ms",
                  "judge service should enforce a runtime limit");
  expect_contains(judge_service, "RLIMIT_AS",
                  "judge service should enforce a memory limit");
  expect_contains(judge_service, "RLIMIT_FSIZE",
                  "judge service should enforce an output size limit");
  expect_contains(judge_service, "kMaxConcurrentJudges",
                  "judge service should limit concurrent judging");
  expect_contains(judge_service, "remove_all(path_",
                  "judge service should clean temporary files");
  expect_contains(comparator, R"(compare_mode == "float_1")",
                  "judge comparator should support float_1 mode");
  expect_contains(comparator, "return actual == expected",
                  "judge comparator should support strict mode");
  expect_contains(admin_js, "/api/admin/me",
                  "admin pages should check admin login state");
  expect_contains(admin_js, "data-delete-id",
                  "admin frontend should expose delete action");
  expect_contains(admin_js, "hidden_testcases",
                  "new problem form should send hidden testcases");
  expect_contains(storage, "oj_problem_status",
                  "frontend should use SPEC localStorage key");
  expect_contains(storage, "\"passed\"",
                  "frontend should persist passed status value");
  expect_contains(problem_detail, "submitButton.disabled = true",
                  "detail page should disable submit when anonymous");
  expect_contains(problem_detail, "status_text",
                  "detail page should show detailed judge status text");
  expect_contains(problem_detail, "markSolved",
                  "detail page should mark solved problems after pass");
  expect_contains(problem_list, "currentUser",
                  "list page should check login state before status display");
}

void test_spec_progress(const fs::path& root) {
  const std::string spec = read_text(root / "SPEC.md");
  expect_contains(spec, "- [x] 实现 GET /api/problems",
                  "SPEC 12.4 problem list item should be complete");
  expect_contains(spec, "- [x] 实现 POST /api/user/login",
                  "SPEC 12.4 login item should be complete");
  expect_contains(spec, "- [x] 实现 POST /api/submit",
                  "SPEC 12.4 submit item should be complete");
  expect_contains(spec, "- [x] 实现 localStorage 完成状态",
                  "SPEC 12.4 localStorage item should be complete");
  expect_contains(spec, "- [x] 实现 POST /api/admin/login",
                  "SPEC 12.5 admin login item should be complete");
  expect_contains(spec, "- [x] 实现 POST /api/admin/problems",
                  "SPEC 12.5 admin create item should be complete");
  expect_contains(spec, "- [x] 实现管理员 session/cookie",
                  "SPEC 12.5 admin session item should be complete");
  expect_contains(spec, "- [x] 实现 DELETE /api/admin/problems/{id}",
                  "SPEC 12.5 admin delete item should be complete");
  expect_contains(spec, "- [x] 创建临时工作目录",
                  "SPEC 12.6 temp directory item should be complete");
  expect_contains(spec, "- [x] 调用 g++ 编译",
                  "SPEC 12.6 compile item should be complete");
  expect_contains(spec, "- [x] 限制运行时间",
                  "SPEC 12.6 time limit item should be complete");
  expect_contains(spec, "- [x] 限制内存",
                  "SPEC 12.6 memory limit item should be complete");
  expect_contains(spec, "- [x] 限制输出大小",
                  "SPEC 12.6 output limit item should be complete");
  expect_contains(spec, "- [x] 实现 strict 比较",
                  "SPEC 12.6 strict compare item should be complete");
  expect_contains(spec, "- [x] 实现 float_1 比较",
                  "SPEC 12.6 float compare item should be complete");
  expect_contains(spec, "- [x] 实现并发判题限制",
                  "SPEC 12.6 concurrency item should be complete");
  expect_contains(spec, "- [x] 清理临时文件",
                  "SPEC 12.6 cleanup item should be complete");
  expect_contains(spec, "status_text",
                  "SPEC should document detailed submit status text");
}

}  // namespace

int main(int argc, char* argv[]) {
  const fs::path root =
      argc >= 2 ? fs::absolute(argv[1]) : fs::current_path();

  try {
    test_seed_password_hashes();
    test_user_session_cookie();
    test_output_comparator();
    test_user_feature_sources(root);
    test_spec_progress(root);
  } catch (const std::exception& error) {
    std::cerr << "FAIL: unexpected exception: " << error.what() << '\n';
    return 1;
  }

  if (failures != 0) {
    std::cerr << failures << " user feature test(s) failed\n";
    return 1;
  }

  std::cout << "PASS: SPEC 12.4 ordinary user and 12.5 admin feature checks passed\n";
  return 0;
}

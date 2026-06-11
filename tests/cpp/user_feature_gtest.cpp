#include "auth/password.h"
#include "auth/session.h"
#include "judge/comparator.h"
#include "judge/judge_service.h"
#include "model/problem.h"
#include "model/testcase.h"

#include <gtest/gtest.h>
#include <httplib.h>

#include <chrono>
#include <cstdint>
#include <filesystem>
#include <fstream>
#include <set>
#include <sstream>
#include <stdexcept>
#include <string>
#include <utility>
#include <vector>

namespace {

namespace fs = std::filesystem;

fs::path project_root = fs::current_path();

std::string read_text(const fs::path& path) {
  std::ifstream input(path);
  if (!input) {
    throw std::runtime_error("failed to read " + path.string());
  }

  std::ostringstream output;
  output << input.rdbuf();
  return output.str();
}

void expect_contains(const std::string& text, const std::string& needle) {
  EXPECT_NE(text.find(needle), std::string::npos)
      << "missing expected text: " << needle;
}

oj::model::Problem addition_problem(std::string compare_mode = "strict") {
  oj::model::Problem problem;
  problem.id = 1;
  problem.title = "A+B";
  problem.difficulty = "easy";
  problem.time_limit_ms = 1000;
  problem.memory_limit_kb = 131072;
  problem.compare_mode = std::move(compare_mode);
  return problem;
}

std::vector<oj::model::Testcase> one_hidden_test() {
  return {
      oj::model::Testcase{1, 1, "1 2\n", "3\n", false},
  };
}

oj::judge::JudgeResult judge_result(
    oj::judge::JudgeService& judge_service, const oj::model::Problem& problem,
    const std::vector<oj::model::Testcase>& testcases, const std::string& code) {
  return judge_service.judge(problem, testcases, code).result;
}

std::set<std::string> judge_temp_children() {
  const fs::path base = fs::absolute("var/judge_tmp");
  std::set<std::string> entries;
  std::error_code error;
  if (!fs::exists(base, error)) {
    return entries;
  }

  fs::directory_iterator it(base, error);
  const fs::directory_iterator end;
  while (!error && it != end) {
    entries.insert(it->path().filename().string());
    it.increment(error);
  }
  return entries;
}

TEST(UserPasswordTest, VerifiesSeededAndGeneratedHashes) {
  constexpr const char* kSeedHash =
      "pbkdf2_sha256$120000$oj-user1-v1$"
      "559a3423ecafda16f42a4b30389959d58e637b750cbb65eb77edb31333bdf2c4";

  EXPECT_TRUE(oj::auth::verify_password("password", kSeedHash));
  EXPECT_FALSE(oj::auth::verify_password("wrong", kSeedHash));

  const std::string generated = oj::auth::hash_password("new-password");
  EXPECT_TRUE(oj::auth::verify_password("new-password", generated));
  EXPECT_FALSE(oj::auth::verify_password("other-password", generated));
}

TEST(UserSessionTest, ManagesCookieBackedSessionLifecycle) {
  oj::auth::SessionStore sessions;
  const std::string session_id = sessions.create_user_session(7, "user1");

  const auto user = sessions.find_user_session(session_id);
  ASSERT_TRUE(user.has_value());
  EXPECT_EQ(user->id, std::uint64_t{7});
  EXPECT_EQ(user->username, "user1");

  httplib::Request request;
  request.set_header("Cookie",
                     "theme=light; oj_user_session=" + session_id +
                         "; locale=zh-CN");
  EXPECT_EQ(oj::auth::cookie_value(request, "oj_user_session"), session_id);
  EXPECT_FALSE(oj::auth::cookie_value(request, "missing").has_value());

  httplib::Response login_response;
  oj::auth::set_user_session_cookie(login_response, session_id);
  const std::string login_cookie =
      login_response.get_header_value("Set-Cookie");
  expect_contains(login_cookie, "oj_user_session=" + session_id);
  expect_contains(login_cookie, "Path=/");
  expect_contains(login_cookie, "HttpOnly");
  expect_contains(login_cookie, "SameSite=Lax");

  httplib::Response logout_response;
  oj::auth::clear_user_session_cookie(logout_response);
  const std::string logout_cookie =
      logout_response.get_header_value("Set-Cookie");
  expect_contains(logout_cookie, "oj_user_session=");
  expect_contains(logout_cookie, "Max-Age=0");

  sessions.destroy_user_session(session_id);
  EXPECT_FALSE(sessions.find_user_session(session_id).has_value());
}

TEST(AdminSessionTest, UsesSeparateCookieBackedSessionLifecycle) {
  oj::auth::SessionStore sessions;
  const std::string session_id = sessions.create_admin_session(3, "admin");

  const auto admin = sessions.find_admin_session(session_id);
  ASSERT_TRUE(admin.has_value());
  EXPECT_EQ(admin->id, std::uint64_t{3});
  EXPECT_EQ(admin->username, "admin");

  httplib::Request request;
  request.set_header("Cookie", "oj_admin_session=" + session_id);
  EXPECT_EQ(oj::auth::cookie_value(request, "oj_admin_session"), session_id);

  httplib::Response login_response;
  oj::auth::set_admin_session_cookie(login_response, session_id);
  const std::string login_cookie =
      login_response.get_header_value("Set-Cookie");
  expect_contains(login_cookie, "oj_admin_session=" + session_id);
  expect_contains(login_cookie, "HttpOnly");
  expect_contains(login_cookie, "SameSite=Lax");

  httplib::Response logout_response;
  oj::auth::clear_admin_session_cookie(logout_response);
  expect_contains(logout_response.get_header_value("Set-Cookie"),
                  "oj_admin_session=");
  expect_contains(logout_response.get_header_value("Set-Cookie"),
                  "Max-Age=0");

  sessions.destroy_admin_session(session_id);
  EXPECT_FALSE(sessions.find_admin_session(session_id).has_value());
}

TEST(SubmitApiContractTest, RejectsAnonymousSubmitBeforeParsingOrDatabase) {
  const std::string submit_api =
      read_text(project_root / "src/api/submit_api.cpp");

  const auto route_pos = submit_api.find(R"(server.Post("/api/submit")");
  const auto auth_pos =
      submit_api.find("if (!has_submit_session(request, sessions))");
  const auto parse_pos = submit_api.find("parse_json_body");
  const auto db_pos = submit_api.find("db::MySqlClient client(mysql_config)");

  ASSERT_NE(route_pos, std::string::npos);
  ASSERT_NE(auth_pos, std::string::npos);
  ASSERT_NE(parse_pos, std::string::npos);
  ASSERT_NE(db_pos, std::string::npos);
  EXPECT_LT(route_pos, auth_pos);
  EXPECT_LT(auth_pos, parse_pos);
  EXPECT_LT(auth_pos, db_pos);
  expect_contains(submit_api, "current_user(request, sessions).has_value()");
  expect_contains(submit_api, "current_admin(request, sessions).has_value()");
  expect_contains(submit_api, "httplib::StatusCode::Unauthorized_401");
  expect_contains(submit_api, R"("unauthorized")");
  expect_contains(submit_api, "JudgeService");
}

TEST(OutputComparatorTest, SupportsStrictAndOneDecimalComparison) {
  EXPECT_TRUE(oj::judge::compare_output("3\n", "3\n", "strict"));
  EXPECT_FALSE(oj::judge::compare_output("3", "3\n", "strict"));

  EXPECT_TRUE(oj::judge::compare_output("2.34\n", "2.3\n", "float_1"));
  EXPECT_TRUE(oj::judge::compare_output("1.24 2.25\n", "1.2 2.3\n",
                                        "float_1"));
  EXPECT_FALSE(oj::judge::compare_output("2.36\n", "2.3\n", "float_1"));
  EXPECT_FALSE(oj::judge::compare_output("ok 1\n", "ok 1 2\n", "float_1"));
}

TEST(JudgeServiceTest, RejectsMissingCodeOrHiddenTestcasesWithoutExecution) {
  oj::judge::JudgeService judge_service;
  const auto problem = addition_problem();
  const auto tests = one_hidden_test();
  constexpr const char* kValidLookingCode = "int main(){return 0;}";

  EXPECT_EQ(judge_result(judge_service, problem, {}, kValidLookingCode),
            oj::judge::JudgeResult::SystemError);
  EXPECT_EQ(judge_result(judge_service, problem, tests, ""),
            oj::judge::JudgeResult::CompileError);

  const std::string judge_service_source =
      read_text(project_root / "src/judge/judge_service.cpp");
  expect_contains(judge_service_source, "compile_code(temp.path())");
  expect_contains(judge_service_source, "run_testcase");
  expect_contains(judge_service_source, "compare_output");
}

TEST(JudgeServiceTest, CompilesAndRunsHiddenTestcases) {
  oj::judge::JudgeService judge_service;
  const auto problem = addition_problem();
  const std::vector<oj::model::Testcase> tests{
      {1, 1, "10 20\n", "30\n", false},
      {2, 1, "-5 8\n", "3\n", false},
  };

  constexpr const char* kAcceptedCode =
      "#include <bits/stdc++.h>\n"
      "using namespace std;\n"
      "int main(){int a,b;if(cin>>a>>b){cout<<a+b<<endl;}return 0;}\n";
  constexpr const char* kWrongAnswerCode =
      "#include <bits/stdc++.h>\n"
      "using namespace std;\n"
      "int main(){cout<<0<<endl;return 0;}\n";

  EXPECT_EQ(judge_result(judge_service, problem, tests, kAcceptedCode),
            oj::judge::JudgeResult::Passed);
  EXPECT_EQ(judge_result(judge_service, problem, tests, kWrongAnswerCode),
            oj::judge::JudgeResult::WrongAnswer);
}

TEST(JudgeServiceTest, RejectsCompilationErrorsAndCleansTempDirectory) {
  oj::judge::JudgeService judge_service;
  const auto problem = addition_problem();
  const auto tests = one_hidden_test();
  const auto before = judge_temp_children();

  EXPECT_EQ(judge_result(judge_service, problem, tests,
                         "int main( { return 0; }"),
            oj::judge::JudgeResult::CompileError);
  EXPECT_EQ(judge_temp_children(), before);
}

TEST(JudgeServiceTest, AppliesFloatOneModeDuringJudge) {
  oj::judge::JudgeService judge_service;
  const auto problem = addition_problem("float_1");
  const std::vector<oj::model::Testcase> tests{
      {1, 1, "", "2.3\n", false},
  };

  constexpr const char* kAcceptedCode =
      "#include <iostream>\n"
      "int main(){std::cout << 2.34 << '\\n'; return 0;}\n";
  constexpr const char* kRejectedCode =
      "#include <iostream>\n"
      "int main(){std::cout << 2.36 << '\\n'; return 0;}\n";

  EXPECT_EQ(judge_result(judge_service, problem, tests, kAcceptedCode),
            oj::judge::JudgeResult::Passed);
  EXPECT_EQ(judge_result(judge_service, problem, tests, kRejectedCode),
            oj::judge::JudgeResult::WrongAnswer);
}

TEST(JudgeServiceTest, EnforcesRuntimeResourceLimits) {
  oj::judge::JudgeService judge_service;
  const auto tests = one_hidden_test();

  auto timeout_problem = addition_problem();
  timeout_problem.time_limit_ms = 50;
  constexpr const char* kTimeoutCode =
      "#include <cstdint>\n"
      "int main(){volatile std::uint64_t x = 0; while (true) { ++x; }}\n";
  EXPECT_EQ(judge_result(judge_service, timeout_problem, tests, kTimeoutCode),
            oj::judge::JudgeResult::TimeLimitExceeded);

  auto memory_problem = addition_problem();
  memory_problem.memory_limit_kb = 65536;
  constexpr const char* kMemoryLimitCode =
      "#include <iostream>\n"
      "#include <vector>\n"
      "int main(){std::vector<char> data(200 * 1024 * 1024);"
      "std::cout << data.size() << '\\n'; return 0;}\n";
  EXPECT_EQ(judge_result(judge_service, memory_problem, tests, kMemoryLimitCode),
            oj::judge::JudgeResult::MemoryLimitExceeded);

  auto output_problem = addition_problem();
  constexpr const char* kOutputLimitCode =
      "#include <iostream>\n"
      "#include <string>\n"
      "int main(){std::cout << std::string(1100000, 'x'); return 0;}\n";
  EXPECT_EQ(judge_result(judge_service, output_problem, tests, kOutputLimitCode),
            oj::judge::JudgeResult::OutputLimitExceeded);
}

TEST(FrontendUserFeatureTest, ImplementsProblemPagesAndSolvedStateContract) {
  const std::string storage = read_text(project_root / "public/js/storage.js");
  const std::string problem_detail =
      read_text(project_root / "public/js/problem-detail.js");
  const std::string problem_list =
      read_text(project_root / "public/js/problem-list.js");
  const std::string api = read_text(project_root / "public/js/api.js");
  const std::string auth = read_text(project_root / "public/js/auth.js");

  expect_contains(api, "getProblems()");
  expect_contains(api, "getProblem(id)");
  expect_contains(api, "/api/user/me");
  expect_contains(api, "/api/user/logout");
  expect_contains(auth, "/api/user/login");
  expect_contains(auth, "/api/user/register");
  expect_contains(problem_detail, "submitButton.disabled = true");
  expect_contains(problem_detail, R"(/api/submit)");
  expect_contains(problem_detail, "status_text");
  expect_contains(problem_detail, "solvedStorage.markSolved(problemId)");
  expect_contains(problem_list, "solvedStorage.isSolved");
  expect_contains(storage, R"(key: "oj_problem_status")");
  expect_contains(storage, R"(solved[problemId] = "passed")");
}

TEST(AdminFeatureContractTest, ImplementsAdminApiAndFrontendFlow) {
  const std::string server = read_text(project_root / "src/app/server.cpp");
  const std::string admin_api =
      read_text(project_root / "src/api/admin_api.cpp");
  const std::string admin_repository =
      read_text(project_root / "src/db/admin_repository.cpp");
  const std::string problem_repository =
      read_text(project_root / "src/db/problem_repository.cpp");
  const std::string admin_js = read_text(project_root / "public/js/admin.js");
  const std::string api_js = read_text(project_root / "public/js/api.js");

  expect_contains(server, "register_admin_routes");
  expect_contains(admin_repository, "FROM admins WHERE username");
  expect_contains(admin_api, R"(/api/admin/login)");
  expect_contains(admin_api, R"(/api/admin/logout)");
  expect_contains(admin_api, R"(/api/admin/problems)");
  expect_contains(admin_api, "oj_admin_session");
  expect_contains(admin_api, "create_admin_session");
  expect_contains(admin_api, "create_with_testcases");
  expect_contains(admin_api, "delete_by_id");
  expect_contains(problem_repository, "START TRANSACTION");
  expect_contains(problem_repository, "INSERT INTO testcases");
  expect_contains(problem_repository, "DELETE FROM problems WHERE id");
  expect_contains(api_js, "delete(path)");
  expect_contains(admin_js, "/api/admin/me");
  expect_contains(admin_js, "/api/admin/login");
  expect_contains(admin_js, "/api/admin/problems");
  expect_contains(admin_js, "data-delete-id");
  expect_contains(admin_js, "hidden_testcases");
}

TEST(SpecProgressTest, MarksOrdinaryUserAdminAndJudgeFeaturesComplete) {
  const std::string spec = read_text(project_root / "SPEC.md");

  expect_contains(spec, "- [x] 实现 GET /api/problems");
  expect_contains(spec, "- [x] 实现 GET /api/problems/{id}");
  expect_contains(spec, "- [x] 实现 POST /api/user/login");
  expect_contains(spec, "- [x] 实现 POST /api/user/logout");
  expect_contains(spec, "- [x] 实现普通用户 session/cookie");
  expect_contains(spec, "- [x] 实现 POST /api/submit");
  expect_contains(spec, "- [x] 实现未登录用户禁止提交");
  expect_contains(spec, "- [x] 实现题目列表页面");
  expect_contains(spec, "- [x] 实现题目详情页面");
  expect_contains(spec, "- [x] 实现 localStorage 完成状态");
  expect_contains(spec, "- [x] 实现 POST /api/admin/login");
  expect_contains(spec, "- [x] 实现 POST /api/admin/logout");
  expect_contains(spec, "- [x] 实现管理员 session/cookie");
  expect_contains(spec, "- [x] 实现 POST /api/admin/problems");
  expect_contains(spec, "- [x] 实现 DELETE /api/admin/problems/{id}");
  expect_contains(spec, "- [x] 实现管理员登录页");
  expect_contains(spec, "- [x] 实现管理员后台页");
  expect_contains(spec, "- [x] 实现新增题目页");
  expect_contains(spec, "- [x] 实现删除题目功能");
  expect_contains(spec, "- [x] 创建临时工作目录");
  expect_contains(spec, "- [x] 写入用户代码文件");
  expect_contains(spec, "- [x] 调用 g++ 编译");
  expect_contains(spec, "- [x] 捕获编译结果");
  expect_contains(spec, "- [x] 执行用户程序");
  expect_contains(spec, "- [x] 传入测试用例 stdin");
  expect_contains(spec, "- [x] 捕获 stdout");
  expect_contains(spec, "- [x] 限制运行时间");
  expect_contains(spec, "- [x] 限制内存");
  expect_contains(spec, "- [x] 限制输出大小");
  expect_contains(spec, "- [x] 实现 strict 比较");
  expect_contains(spec, "- [x] 实现 float_1 比较");
  expect_contains(spec, "- [x] 实现并发判题限制");
  expect_contains(spec, "- [x] 清理临时文件");
}

}  // namespace

int main(int argc, char** argv) {
  testing::InitGoogleTest(&argc, argv);
  if (argc >= 2) {
    project_root = fs::absolute(argv[1]);
  }

  return RUN_ALL_TESTS();
}

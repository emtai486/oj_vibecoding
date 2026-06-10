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

void expect_non_empty_file(const fs::path& root,
                           const std::string& relative_path) {
  const fs::path path = root / relative_path;
  expect(fs::is_regular_file(path), relative_path + " should be a file");
  if (fs::is_regular_file(path)) {
    expect(fs::file_size(path) > 0, relative_path + " should not be empty");
  }
}

void expect_executable_script(const fs::path& root,
                              const std::string& relative_path) {
  expect_non_empty_file(root, relative_path);
  const fs::path path = root / relative_path;
  if (!fs::is_regular_file(path)) {
    return;
  }

  const std::string content = read_text(path);
  expect_contains(content, "#!/usr/bin/env bash",
                  relative_path + " should be a bash script");
  expect_contains(content, "set -euo pipefail",
                  relative_path + " should fail fast");

  const auto permissions = fs::status(path).permissions();
  expect((permissions & fs::perms::owner_exec) != fs::perms::none,
         relative_path + " should be executable by the owner");
}

void test_deployment_scripts(const fs::path& root) {
  expect_executable_script(root, "scripts/build.sh");
  expect_executable_script(root, "scripts/start_server.sh");
  expect_executable_script(root, "scripts/init_db.sh");
  expect_executable_script(root, "scripts/deploy_verify.sh");

  const std::string build = read_text(root / "scripts/build.sh");
  expect_contains(build, "command -v make",
                  "build script should check make");
  expect_contains(build, "command -v g++",
                  "build script should check g++");
  expect_contains(build, "command -v mysql_config",
                  "build script should check mysql_config");
  expect_contains(build, "make all", "build script should run make all");
  expect_contains(build, "build/oj_server",
                  "build script should verify the backend binary");

  const std::string start = read_text(root / "scripts/start_server.sh");
  expect_contains(start, "CONFIG_PATH",
                  "start script should accept a config path");
  expect_contains(start, "exec ./build/oj_server \"$CONFIG_PATH\"",
                  "start script should exec the backend binary");

  const std::string init_db = read_text(root / "scripts/init_db.sh");
  expect_contains(init_db, "command -v mysql",
                  "database init script should check mysql client");
  expect_contains(init_db, "CREATE DATABASE IF NOT EXISTS",
                  "database init script should create the database");
  expect_contains(init_db, "--defaults-extra-file",
                  "database init script should avoid exposing passwords");
  expect_contains(init_db, "sql/schema.sql",
                  "database init script should import schema.sql");
  expect_contains(init_db, "sql/seed.sql",
                  "database init script should import seed.sql");

  const std::string deploy = read_text(root / "scripts/deploy_verify.sh");
  expect_contains(deploy, "--strict-os",
                  "deployment verification should support strict OS checks");
  expect_contains(deploy, "Ubuntu 22.04",
                  "deployment verification should validate Ubuntu 22.04");
  expect_contains(deploy, "check_not_root",
                  "deployment verification should reject root service runs");
  expect_contains(deploy, "bash scripts/build.sh",
                  "deployment verification should run the build script");
  expect_contains(deploy, "verify_cpp_compile_run",
                  "deployment verification should compile and run C++ code");
  expect_contains(deploy, "g++ -std=c++17",
                  "deployment verification should verify C++17 compilation");
  expect_contains(deploy, "bash scripts/init_db.sh",
                  "deployment verification should initialize the database");
  expect_contains(deploy, "--check-db",
                  "deployment verification should check MySQL connectivity");
  expect_contains(deploy, "verify_static_assets",
                  "deployment verification should load static assets");
  expect_contains(deploy, "python3 scripts/api_python_test.py",
                  "deployment verification should run API workflows");
}

void test_make_targets(const fs::path& root) {
  const std::string makefile = read_text(root / "Makefile");
  expect_contains(makefile, "deploy-build:",
                  "Makefile should expose deploy-build");
  expect_contains(makefile, "bash scripts/build.sh",
                  "deploy-build should call the build script");
  expect_contains(makefile, "deploy-init-db:",
                  "Makefile should expose deploy-init-db");
  expect_contains(makefile, "bash scripts/init_db.sh config/app.conf",
                  "deploy-init-db should call the database init script");
  expect_contains(makefile, "deploy-start:",
                  "Makefile should expose deploy-start");
  expect_contains(makefile, "bash scripts/start_server.sh config/app.conf",
                  "deploy-start should call the start script");
  expect_contains(makefile, "deploy-verify:",
                  "Makefile should expose deploy-verify");
  expect_contains(makefile, "bash scripts/deploy_verify.sh config/app.conf",
                  "deploy-verify should call the full verification script");
  expect_contains(makefile, "deploy-verify-basic:",
                  "Makefile should expose deploy-verify-basic");
  expect_contains(makefile,
                  "bash scripts/deploy_verify.sh --basic config/app.example.conf",
                  "deploy-verify-basic should avoid DB-backed checks");
  expect_contains(makefile, "deploy-verify-strict:",
                  "Makefile should expose deploy-verify-strict");
  expect_contains(makefile,
                  "bash scripts/deploy_verify.sh --strict-os config/app.conf",
                  "deploy-verify-strict should enforce Ubuntu 22.04");
  expect_contains(makefile, "DEPLOYMENT_ACCEPTANCE_TEST_TARGET",
                  "make test should include deployment acceptance checks");
}

void test_acceptance_automation(const fs::path& root) {
  const std::string api_python =
      read_text(root / "scripts/api_python_test.py");
  expect_contains(api_python, "POST /api/submit anonymous",
                  "API tests should verify anonymous submit rejection");
  expect_contains(api_python, "POST /api/user/login",
                  "API tests should verify ordinary user login");
  expect_contains(api_python, "POST /api/submit accepted code",
                  "API tests should verify ordinary user accepted submission");
  expect_contains(api_python, "POST /api/submit compile error",
                  "API tests should verify compile failure");
  expect_contains(api_python, "POST /api/submit timeout",
                  "API tests should verify timeout failure");
  expect_contains(api_python, "POST /api/submit memory limit",
                  "API tests should verify memory limit failure");
  expect_contains(api_python, "POST /api/submit output limit",
                  "API tests should verify output limit failure");
  expect_contains(api_python, "POST /api/admin/problems",
                  "API tests should verify admin problem creation");
  expect_contains(api_python, "DELETE /api/admin/problems/{created}",
                  "API tests should verify admin problem deletion");
  expect_contains(api_python, "hidden testcase mismatch",
                  "API tests should verify hidden testcase judging");

  const std::string problem_detail =
      read_text(root / "public/js/problem-detail.js");
  expect_contains(problem_detail, "submitButton.disabled = true",
                  "frontend should block anonymous submit");
  expect_contains(problem_detail, "solvedStorage.markSolved",
                  "frontend should persist accepted submissions locally");

  const std::string storage = read_text(root / "public/js/storage.js");
  expect_contains(storage, "localStorage",
                  "frontend should use localStorage for solved status");
  expect_contains(storage, "oj_problem_status",
                  "frontend should use the documented solved-status key");
}

void test_readme_deployment_docs(const fs::path& root) {
  const std::string readme = read_text(root / "README.md");
  expect_contains(readme, "Ubuntu 22.04",
                  "README should document the deployment OS");
  expect_contains(readme, "scripts/build.sh",
                  "README should document the build script");
  expect_contains(readme, "scripts/init_db.sh",
                  "README should document the database init script");
  expect_contains(readme, "scripts/start_server.sh",
                  "README should document the start script");
  expect_contains(readme, "scripts/deploy_verify.sh --basic",
                  "README should document basic deployment verification");
  expect_contains(readme, "scripts/deploy_verify.sh --strict-os",
                  "README should document strict Ubuntu 22.04 verification");
  expect_contains(readme, "make deploy-verify",
                  "README should document the Makefile deployment target");
  expect_contains(readme, "make deploy-verify-strict",
                  "README should document strict deployment verification");
}

void test_spec_progress(const fs::path& root) {
  const std::string spec = read_text(root / "SPEC.md");
  const std::vector<std::string> completed_items = {
      "- [x] 编写构建脚本",
      "- [x] 编写启动脚本",
      "- [x] 编写数据库初始化脚本",
      "- [x] 验证 g++ 编译运行",
      "- [x] 验证 MySQL 连接",
      "- [x] 验证前端静态资源本地加载",
      "- [x] 验证普通用户登录后才能提交代码",
      "- [x] 验证管理员新增/删除题目",
      "- [x] 验证普通用户提交代码",
  };

  for (const auto& item : completed_items) {
    expect_contains(spec, item,
                    "SPEC 12.7 deployment item should be complete: " + item);
  }

  expect_contains(spec, "- [x] 在当前项目服务器 Ubuntu 22.04.5 LTS 验证",
                  "SPEC should mark current-server verification complete after strict acceptance runs");
  expect_contains(spec, "scripts/deploy_verify.sh --strict-os",
                  "SPEC should document the strict Ubuntu 22.04 verification command");
}

}  // namespace

int main(int argc, char* argv[]) {
  const fs::path root =
      argc >= 2 ? fs::absolute(argv[1]) : fs::current_path();

  try {
    test_deployment_scripts(root);
    test_make_targets(root);
    test_acceptance_automation(root);
    test_readme_deployment_docs(root);
    test_spec_progress(root);
  } catch (const std::exception& error) {
    std::cerr << "FAIL: unexpected exception: " << error.what() << '\n';
    return 1;
  }

  if (failures != 0) {
    std::cerr << failures << " deployment acceptance test(s) failed\n";
    return 1;
  }

  std::cout << "PASS: SPEC 12.7 deployment acceptance checks passed\n";
  return 0;
}

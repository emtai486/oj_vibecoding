#include "judge/judge_service.h"

#include "judge/comparator.h"

#include <fcntl.h>
#include <signal.h>
#include <sys/resource.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <unistd.h>

#include <chrono>
#include <condition_variable>
#include <cstdint>
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <mutex>
#include <random>
#include <sstream>
#include <string>
#include <thread>

namespace oj::judge {
namespace {

namespace fs = std::filesystem;

constexpr std::uintmax_t kMaxOutputBytes = 1024 * 1024;
constexpr std::uintmax_t kMaxDiagnosticBytes = 8192;
constexpr int kCompileTimeoutMs = 10000;
constexpr std::uint32_t kCompileMemoryKb = 524288;
constexpr int kMaxConcurrentJudges = 4;

class JudgeSlot {
 public:
  JudgeSlot() {
    std::unique_lock<std::mutex> lock(mutex());
    condition().wait(lock, [] { return active_count() < kMaxConcurrentJudges; });
    ++active_count();
  }

  ~JudgeSlot() {
    {
      std::lock_guard<std::mutex> lock(mutex());
      --active_count();
    }
    condition().notify_one();
  }

  JudgeSlot(const JudgeSlot&) = delete;
  JudgeSlot& operator=(const JudgeSlot&) = delete;

 private:
  static std::mutex& mutex() {
    static std::mutex value;
    return value;
  }

  static std::condition_variable& condition() {
    static std::condition_variable value;
    return value;
  }

  static int& active_count() {
    static int value = 0;
    return value;
  }
};

class TempDirectory {
 public:
  TempDirectory() {
    const fs::path base = fs::absolute("var/judge_tmp");
    fs::create_directories(base);
    path_ = base / unique_name();
    fs::create_directories(path_);
  }

  ~TempDirectory() {
    std::error_code error;
    fs::remove_all(path_, error);
  }

  const fs::path& path() const { return path_; }

 private:
  static std::string unique_name() {
    std::random_device random;
    const auto now =
        std::chrono::steady_clock::now().time_since_epoch().count();
    std::ostringstream output;
    output << "judge-" << getpid() << '-' << std::hex << random() << '-'
           << now;
    return output.str();
  }

  fs::path path_;
};

struct ProcessSpec {
  std::vector<std::string> argv;
  fs::path cwd;
  fs::path stdin_path;
  fs::path stdout_path;
  fs::path stderr_path;
  int timeout_ms = 1000;
  std::uint32_t memory_limit_kb = 131072;
  bool limit_output_file = false;
};

struct ProcessResult {
  bool exited_ok = false;
  bool timed_out = false;
  bool output_limit_exceeded = false;
  bool signaled = false;
  int exit_code = -1;
  int signal = 0;
};

struct TestcaseReport {
  JudgeResult result = JudgeResult::SystemError;
  std::string detail;
};

bool write_text(const fs::path& path, const std::string& content) {
  std::ofstream output(path, std::ios::binary);
  if (!output) {
    return false;
  }
  output.write(content.data(), static_cast<std::streamsize>(content.size()));
  return output.good();
}

std::string read_text(const fs::path& path, std::uintmax_t max_bytes) {
  std::ifstream input(path, std::ios::binary);
  if (!input) {
    return "";
  }

  std::ostringstream output;
  char buffer[4096];
  std::uintmax_t total = 0;
  while (input) {
    input.read(buffer, sizeof(buffer));
    const auto count = input.gcount();
    if (count <= 0) {
      break;
    }
    total += static_cast<std::uintmax_t>(count);
    if (total > max_bytes) {
      break;
    }
    output.write(buffer, count);
  }
  return output.str();
}

bool file_within_limit(const fs::path& path, std::uintmax_t max_bytes) {
  std::error_code error;
  const auto size = fs::file_size(path, error);
  return !error && size <= max_bytes;
}

std::string truncate_text(std::string value, std::size_t max_length) {
  if (value.size() <= max_length) {
    return value;
  }

  value.resize(max_length);
  value += "\n... output truncated ...";
  return value;
}

std::string process_failure_detail(const ProcessResult& result,
                                   const fs::path& error_path) {
  std::string detail = read_text(error_path, kMaxDiagnosticBytes);
  if (!detail.empty()) {
    return truncate_text(std::move(detail),
                         static_cast<std::size_t>(kMaxDiagnosticBytes));
  }

  if (result.timed_out) {
    return "process timed out";
  }
  if (result.signaled) {
    return "process terminated by signal " + std::to_string(result.signal);
  }
  if (result.exit_code >= 0) {
    return "process exited with code " + std::to_string(result.exit_code);
  }
  return "process failed";
}

void redirect_fd(const fs::path& path, int flags, int target_fd) {
  const int fd = ::open(path.c_str(), flags, 0600);
  if (fd < 0) {
    _exit(126);
  }
  if (::dup2(fd, target_fd) < 0) {
    _exit(126);
  }
  ::close(fd);
}

void apply_limits(std::uint32_t memory_limit_kb, bool limit_output_file) {
  rlimit as_limit{};
  as_limit.rlim_cur = static_cast<rlim_t>(memory_limit_kb) * 1024;
  as_limit.rlim_max = as_limit.rlim_cur;
  (void)::setrlimit(RLIMIT_AS, &as_limit);

  if (limit_output_file) {
    rlimit file_limit{};
    file_limit.rlim_cur = static_cast<rlim_t>(kMaxOutputBytes + 1);
    file_limit.rlim_max = file_limit.rlim_cur;
    (void)::setrlimit(RLIMIT_FSIZE, &file_limit);
  }
}

ProcessResult run_process(const ProcessSpec& spec) {
  ProcessResult result;
  if (spec.argv.empty()) {
    return result;
  }

  const pid_t pid = ::fork();
  if (pid < 0) {
    return result;
  }

  if (pid == 0) {
    (void)::setpgid(0, 0);
    if (!spec.cwd.empty() && ::chdir(spec.cwd.c_str()) != 0) {
      _exit(126);
    }

    if (!spec.stdin_path.empty()) {
      redirect_fd(spec.stdin_path, O_RDONLY, STDIN_FILENO);
    } else {
      redirect_fd("/dev/null", O_RDONLY, STDIN_FILENO);
    }
    redirect_fd(spec.stdout_path, O_WRONLY | O_CREAT | O_TRUNC, STDOUT_FILENO);
    redirect_fd(spec.stderr_path, O_WRONLY | O_CREAT | O_TRUNC, STDERR_FILENO);
    apply_limits(spec.memory_limit_kb, spec.limit_output_file);

    std::vector<char*> argv;
    argv.reserve(spec.argv.size() + 1);
    for (const auto& arg : spec.argv) {
      argv.push_back(const_cast<char*>(arg.c_str()));
    }
    argv.push_back(nullptr);
    ::execvp(argv[0], argv.data());
    _exit(127);
  }

  (void)::setpgid(pid, pid);
  const auto deadline =
      std::chrono::steady_clock::now() + std::chrono::milliseconds(spec.timeout_ms);
  int status = 0;
  while (true) {
    const pid_t waited = ::waitpid(pid, &status, WNOHANG);
    if (waited == pid) {
      if (WIFEXITED(status)) {
        result.exit_code = WEXITSTATUS(status);
        result.exited_ok = result.exit_code == 0;
      } else if (WIFSIGNALED(status)) {
        result.signaled = true;
        result.signal = WTERMSIG(status);
        result.output_limit_exceeded = result.signal == SIGXFSZ;
      }
      return result;
    }
    if (waited < 0) {
      return result;
    }
    if (std::chrono::steady_clock::now() >= deadline) {
      result.timed_out = true;
      ::kill(-pid, SIGKILL);
      ::kill(pid, SIGKILL);
      (void)::waitpid(pid, &status, 0);
      return result;
    }
    std::this_thread::sleep_for(std::chrono::milliseconds(10));
  }
}

ProcessResult compile_code(const fs::path& dir) {
  const ProcessSpec spec{
      {"g++", "main.cpp", "-std=c++17", "-O2", "-pipe", "-o", "main"},
      dir,
      {},
      dir / "compile.out",
      dir / "compile.err",
      kCompileTimeoutMs,
      kCompileMemoryKb,
      true,
  };
  return run_process(spec);
}

bool looks_like_memory_limit(const ProcessResult& result,
                             const fs::path& error_path) {
  if (result.signal == SIGKILL) {
    return true;
  }

  const std::string error = read_text(error_path, 4096);
  return error.find("std::bad_alloc") != std::string::npos ||
         error.find("Cannot allocate memory") != std::string::npos ||
         error.find("cannot allocate memory") != std::string::npos;
}

TestcaseReport run_testcase(const fs::path& dir, const model::Problem& problem,
                            const model::Testcase& testcase,
                            std::size_t index) {
  const fs::path input_path = dir / ("input-" + std::to_string(index) + ".txt");
  const fs::path output_path =
      dir / ("output-" + std::to_string(index) + ".txt");
  const fs::path error_path = dir / ("error-" + std::to_string(index) + ".txt");
  if (!write_text(input_path, testcase.input)) {
    return {JudgeResult::SystemError, "failed to write testcase input"};
  }

  const ProcessSpec spec{
      {"./main"},
      dir,
      input_path,
      output_path,
      error_path,
      static_cast<int>(problem.time_limit_ms),
      problem.memory_limit_kb,
      true,
  };

  const auto result = run_process(spec);
  if (result.timed_out) {
    return {JudgeResult::TimeLimitExceeded,
            "program exceeded time limit of " +
                std::to_string(problem.time_limit_ms) + " ms"};
  }
  if (result.output_limit_exceeded ||
      !file_within_limit(output_path, kMaxOutputBytes)) {
    return {JudgeResult::OutputLimitExceeded,
            "program output exceeded 1048576 bytes"};
  }
  if (looks_like_memory_limit(result, error_path)) {
    return {JudgeResult::MemoryLimitExceeded,
            "program exceeded memory limit of " +
                std::to_string(problem.memory_limit_kb) + " KB"};
  }
  if (!result.exited_ok) {
    return {JudgeResult::RuntimeError,
            process_failure_detail(result, error_path)};
  }

  const std::string output = read_text(output_path, kMaxOutputBytes);
  if (!compare_output(output, testcase.expected_output, problem.compare_mode)) {
    return {JudgeResult::WrongAnswer, ""};
  }
  return {JudgeResult::Passed, ""};
}

}  // namespace

std::string judge_result_code(JudgeResult result) {
  switch (result) {
    case JudgeResult::Passed:
      return "accepted";
    case JudgeResult::WrongAnswer:
      return "wrong_answer";
    case JudgeResult::CompileError:
      return "compile_error";
    case JudgeResult::TimeLimitExceeded:
      return "time_limit_exceeded";
    case JudgeResult::MemoryLimitExceeded:
      return "memory_limit_exceeded";
    case JudgeResult::OutputLimitExceeded:
      return "output_limit_exceeded";
    case JudgeResult::RuntimeError:
      return "runtime_error";
    case JudgeResult::SystemError:
      return "system_error";
  }
  return "system_error";
}

std::string judge_result_text(JudgeResult result) {
  switch (result) {
    case JudgeResult::Passed:
      return "Accepted";
    case JudgeResult::WrongAnswer:
      return "Wrong Answer";
    case JudgeResult::CompileError:
      return "Compile Error";
    case JudgeResult::TimeLimitExceeded:
      return "Time Limit Exceeded";
    case JudgeResult::MemoryLimitExceeded:
      return "Memory Limit Exceeded";
    case JudgeResult::OutputLimitExceeded:
      return "Output Limit Exceeded";
    case JudgeResult::RuntimeError:
      return "Runtime Error";
    case JudgeResult::SystemError:
      return "System Error";
  }
  return "System Error";
}

bool judge_result_passed(JudgeResult result) {
  return result == JudgeResult::Passed;
}

JudgeReport JudgeService::judge(
    const model::Problem& problem,
    const std::vector<model::Testcase>& hidden_testcases,
    const std::string& code) {
  if (code.empty()) {
    return {JudgeResult::CompileError, 0, "source code is empty"};
  }
  if (hidden_testcases.empty()) {
    return {JudgeResult::SystemError, 0, "problem has no hidden testcases"};
  }

  JudgeSlot slot;
  TempDirectory temp;
  if (!write_text(temp.path() / "main.cpp", code)) {
    return {JudgeResult::SystemError, 0, "failed to write source file"};
  }

  const auto compile_result = compile_code(temp.path());
  const fs::path compile_error_path = temp.path() / "compile.err";
  if (!compile_result.exited_ok) {
    return {JudgeResult::CompileError, 0,
            process_failure_detail(compile_result, compile_error_path)};
  }

  for (std::size_t i = 0; i < hidden_testcases.size(); ++i) {
    const auto report =
        run_testcase(temp.path(), problem, hidden_testcases[i], i);
    if (!judge_result_passed(report.result)) {
      return {report.result, i + 1, report.detail};
    }
  }

  return {JudgeResult::Passed, hidden_testcases.size(), ""};
}

}  // namespace oj::judge

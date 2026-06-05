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
#include <cstdlib>
#include <cstdint>
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
    fs::create_directories("var/judge_tmp");
    path_ = fs::path("var/judge_tmp") / unique_name();
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
      result.exited_ok = WIFEXITED(status) && WEXITSTATUS(status) == 0;
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

bool compile_code(const fs::path& dir) {
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
  const auto result = run_process(spec);
  return result.exited_ok;
}

bool run_testcase(const fs::path& dir, const model::Problem& problem,
                  const model::Testcase& testcase, std::size_t index) {
  const fs::path input_path = dir / ("input-" + std::to_string(index) + ".txt");
  const fs::path output_path =
      dir / ("output-" + std::to_string(index) + ".txt");
  const fs::path error_path = dir / ("error-" + std::to_string(index) + ".txt");
  if (!write_text(input_path, testcase.input)) {
    return false;
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
  if (!result.exited_ok || result.timed_out ||
      !file_within_limit(output_path, kMaxOutputBytes)) {
    return false;
  }

  const std::string output = read_text(output_path, kMaxOutputBytes);
  return compare_output(output, testcase.expected_output, problem.compare_mode);
}

}  // namespace

JudgeResult JudgeService::judge(
    const model::Problem& problem,
    const std::vector<model::Testcase>& hidden_testcases,
    const std::string& code) {
  if (code.empty() || hidden_testcases.empty()) {
    return JudgeResult::Failed;
  }

  JudgeSlot slot;
  TempDirectory temp;
  if (!write_text(temp.path() / "main.cpp", code)) {
    return JudgeResult::Failed;
  }

  if (!compile_code(temp.path())) {
    return JudgeResult::Failed;
  }

  for (std::size_t i = 0; i < hidden_testcases.size(); ++i) {
    if (!run_testcase(temp.path(), problem, hidden_testcases[i], i)) {
      return JudgeResult::Failed;
    }
  }

  return JudgeResult::Passed;
}

}  // namespace oj::judge

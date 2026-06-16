#pragma once

#include "model/problem.h"
#include "model/testcase.h"

#include <cstddef>
#include <string>
#include <vector>

namespace oj::judge {

enum class JudgeResult {
  Passed,
  WrongAnswer,
  CompileError,
  TimeLimitExceeded,
  MemoryLimitExceeded,
  OutputLimitExceeded,
  RuntimeError,
  SystemError,
};

struct JudgeReport {
  JudgeResult result = JudgeResult::SystemError;
  std::size_t testcase_index = 0;
  std::string detail;
};

std::string judge_result_code(JudgeResult result);
std::string judge_result_text(JudgeResult result);
bool judge_result_passed(JudgeResult result);

class JudgeService {
 public:
  JudgeReport judge(const model::Problem& problem,
                    const std::vector<model::Testcase>& hidden_testcases,
                    const std::string& code);
};

}  // namespace oj::judge

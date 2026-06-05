#pragma once

#include "model/problem.h"
#include "model/testcase.h"

#include <string>
#include <vector>

namespace oj::judge {

enum class JudgeResult {
  Passed,
  Failed,
};

class JudgeService {
 public:
  JudgeResult judge(const model::Problem& problem,
                    const std::vector<model::Testcase>& hidden_testcases,
                    const std::string& code);
};

}  // namespace oj::judge

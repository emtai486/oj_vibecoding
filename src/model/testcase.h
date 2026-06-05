#pragma once

#include <cstdint>
#include <string>

namespace oj::model {

struct Testcase {
  std::uint64_t id = 0;
  std::uint64_t problem_id = 0;
  std::string input;
  std::string expected_output;
  bool is_sample = false;
};

}  // namespace oj::model

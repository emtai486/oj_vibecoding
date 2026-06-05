#pragma once

#include <cstdint>
#include <string>

namespace oj::model {

struct ProblemSummary {
  std::uint64_t id = 0;
  std::string title;
  std::string difficulty;
};

struct Problem {
  std::uint64_t id = 0;
  std::string title;
  std::string difficulty;
  std::string description;
  std::string input_format;
  std::string output_format;
  std::string sample_input;
  std::string sample_output;
  std::uint32_t time_limit_ms = 1000;
  std::uint32_t memory_limit_kb = 131072;
  std::string compare_mode = "strict";
};

}  // namespace oj::model

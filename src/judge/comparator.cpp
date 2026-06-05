#include "judge/comparator.h"

#include <cmath>
#include <cstdlib>
#include <sstream>
#include <string_view>
#include <vector>

namespace oj::judge {
namespace {

std::vector<std::string> split_tokens(const std::string& value) {
  std::istringstream input(value);
  std::vector<std::string> tokens;
  std::string token;
  while (input >> token) {
    tokens.push_back(std::move(token));
  }
  return tokens;
}

bool parse_number(const std::string& token, double* value) {
  char* end = nullptr;
  const double parsed = std::strtod(token.c_str(), &end);
  if (end == token.c_str() || *end != '\0') {
    return false;
  }

  if (value != nullptr) {
    *value = parsed;
  }
  return true;
}

bool compare_float_1(const std::string& actual, const std::string& expected) {
  const auto actual_tokens = split_tokens(actual);
  const auto expected_tokens = split_tokens(expected);
  if (actual_tokens.size() != expected_tokens.size()) {
    return false;
  }

  for (std::size_t i = 0; i < actual_tokens.size(); ++i) {
    double actual_number = 0.0;
    double expected_number = 0.0;
    const bool actual_is_number = parse_number(actual_tokens[i], &actual_number);
    const bool expected_is_number =
        parse_number(expected_tokens[i], &expected_number);

    if (actual_is_number || expected_is_number) {
      if (!actual_is_number || !expected_is_number) {
        return false;
      }
      if (std::llround(actual_number * 10.0) !=
          std::llround(expected_number * 10.0)) {
        return false;
      }
      continue;
    }

    if (actual_tokens[i] != expected_tokens[i]) {
      return false;
    }
  }
  return true;
}

}  // namespace

bool compare_output(const std::string& actual, const std::string& expected,
                    const std::string& compare_mode) {
  if (compare_mode == "float_1") {
    return compare_float_1(actual, expected);
  }
  return actual == expected;
}

}  // namespace oj::judge

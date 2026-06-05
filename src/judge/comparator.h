#pragma once

#include <string>

namespace oj::judge {

bool compare_output(const std::string& actual, const std::string& expected,
                    const std::string& compare_mode);

}  // namespace oj::judge

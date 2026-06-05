#pragma once

#include <cstdint>
#include <string>

namespace oj::model {

struct User {
  std::uint64_t id = 0;
  std::string username;
  std::string password_hash;
};

}  // namespace oj::model

#pragma once

#include <string>

namespace oj::auth {

bool verify_password(const std::string& password,
                     const std::string& password_hash);

std::string hash_password(const std::string& password);

}  // namespace oj::auth

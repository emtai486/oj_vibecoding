#pragma once

#include "db/mysql_client.h"
#include "model/user.h"

#include <cstdint>
#include <optional>
#include <string>

namespace oj::db {

class UserRepository {
 public:
  explicit UserRepository(MySqlClient& client);

  bool find_by_username(const std::string& username,
                        std::optional<model::User>* user,
                        std::string* error);
  bool create(const std::string& username, const std::string& password_hash,
              std::uint64_t* id, std::string* error);

 private:
  MySqlClient& client_;
};

}  // namespace oj::db

#pragma once

#include "db/mysql_client.h"
#include "model/user.h"

#include <optional>
#include <string>

namespace oj::db {

class AdminRepository {
 public:
  explicit AdminRepository(MySqlClient& client);

  bool find_by_username(const std::string& username,
                        std::optional<model::User>* admin,
                        std::string* error);

 private:
  MySqlClient& client_;
};

}  // namespace oj::db

#pragma once

#include "config/config.h"

#include <mysql/mysql.h>

#include <string>

namespace oj::db {

class MySqlClient {
 public:
  explicit MySqlClient(config::MySqlConfig config);
  ~MySqlClient();

  MySqlClient(const MySqlClient&) = delete;
  MySqlClient& operator=(const MySqlClient&) = delete;

  MySqlClient(MySqlClient&& other) noexcept;
  MySqlClient& operator=(MySqlClient&& other) noexcept;

  bool connect(std::string* error);
  bool ping(std::string* error) const;

  MYSQL* raw() const noexcept;

 private:
  void close() noexcept;
  void set_error(std::string* error, const std::string& message) const;

  config::MySqlConfig config_;
  MYSQL* connection_ = nullptr;
};

}  // namespace oj::db

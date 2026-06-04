#pragma once

#include "config/config.h"

#include <mysql/mysql.h>

#include <cstdint>
#include <optional>
#include <string>
#include <string_view>
#include <unordered_map>
#include <vector>

namespace oj::db {

struct QueryResult {
  using Row = std::unordered_map<std::string, std::optional<std::string>>;

  std::vector<std::string> columns;
  std::vector<Row> rows;
  std::uint64_t affected_rows = 0;
  std::uint64_t insert_id = 0;
};

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
  bool execute(const std::string& sql, std::string* error);
  bool query(const std::string& sql, QueryResult* result, std::string* error);
  std::string escape(std::string_view value) const;

  bool is_connected() const noexcept;

  MYSQL* raw() const noexcept;

 private:
  void close() noexcept;
  void set_error(std::string* error, const std::string& message) const;

  config::MySqlConfig config_;
  MYSQL* connection_ = nullptr;
};

}  // namespace oj::db

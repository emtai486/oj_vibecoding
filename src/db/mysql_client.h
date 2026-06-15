#pragma once

#include "config/config.h"

#include <mysql/mysql.h>

#include <cstdint>
#include <condition_variable>
#include <deque>
#include <memory>
#include <mutex>
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

class MySqlConnectionPool;

class PooledMySqlClient {
 public:
  PooledMySqlClient() = default;
  ~PooledMySqlClient();

  PooledMySqlClient(const PooledMySqlClient&) = delete;
  PooledMySqlClient& operator=(const PooledMySqlClient&) = delete;

  PooledMySqlClient(PooledMySqlClient&& other) noexcept;
  PooledMySqlClient& operator=(PooledMySqlClient&& other) noexcept;

  MySqlClient* operator->() noexcept;
  const MySqlClient* operator->() const noexcept;
  MySqlClient& operator*() noexcept;
  const MySqlClient& operator*() const noexcept;
  explicit operator bool() const noexcept;

  void reset() noexcept;

 private:
  friend class MySqlConnectionPool;

  PooledMySqlClient(MySqlConnectionPool* pool,
                    std::unique_ptr<MySqlClient> client);

  MySqlConnectionPool* pool_ = nullptr;
  std::unique_ptr<MySqlClient> client_;
};

class MySqlConnectionPool {
 public:
  explicit MySqlConnectionPool(config::MySqlConfig config);

  MySqlConnectionPool(const MySqlConnectionPool&) = delete;
  MySqlConnectionPool& operator=(const MySqlConnectionPool&) = delete;

  bool acquire(PooledMySqlClient* client, std::string* error);
  std::uint32_t max_size() const noexcept;

 private:
  friend class PooledMySqlClient;

  bool ensure_connected(MySqlClient* client, std::string* error) const;
  void release(std::unique_ptr<MySqlClient> client) noexcept;

  config::MySqlConfig config_;
  std::uint32_t max_size_;
  mutable std::mutex mutex_;
  std::condition_variable condition_;
  std::deque<std::unique_ptr<MySqlClient>> idle_;
  std::uint32_t open_count_ = 0;
};

}  // namespace oj::db

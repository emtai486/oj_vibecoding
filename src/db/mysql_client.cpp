#include "db/mysql_client.h"

#include <utility>

namespace oj::db {

MySqlClient::MySqlClient(config::MySqlConfig config)
    : config_(std::move(config)) {}

MySqlClient::~MySqlClient() { close(); }

MySqlClient::MySqlClient(MySqlClient&& other) noexcept
    : config_(std::move(other.config_)), connection_(other.connection_) {
  other.connection_ = nullptr;
}

MySqlClient& MySqlClient::operator=(MySqlClient&& other) noexcept {
  if (this != &other) {
    close();
    config_ = std::move(other.config_);
    connection_ = other.connection_;
    other.connection_ = nullptr;
  }
  return *this;
}

bool MySqlClient::connect(std::string* error) {
  close();

  connection_ = mysql_init(nullptr);
  if (connection_ == nullptr) {
    set_error(error, "mysql_init failed");
    return false;
  }

  unsigned int timeout = config_.connect_timeout_seconds;
  if (mysql_options(connection_, MYSQL_OPT_CONNECT_TIMEOUT, &timeout) != 0) {
    set_error(error, mysql_error(connection_));
    close();
    return false;
  }

  if (!config_.charset.empty() &&
      mysql_options(connection_, MYSQL_SET_CHARSET_NAME,
                    config_.charset.c_str()) != 0) {
    set_error(error, mysql_error(connection_));
    close();
    return false;
  }

  if (mysql_real_connect(connection_, config_.host.c_str(), config_.user.c_str(),
                         config_.password.c_str(), config_.database.c_str(),
                         config_.port, nullptr, 0) == nullptr) {
    set_error(error, mysql_error(connection_));
    close();
    return false;
  }

  if (!config_.charset.empty() &&
      mysql_set_character_set(connection_, config_.charset.c_str()) != 0) {
    set_error(error, mysql_error(connection_));
    close();
    return false;
  }

  return true;
}

bool MySqlClient::ping(std::string* error) const {
  if (connection_ == nullptr) {
    set_error(error, "mysql connection is not open");
    return false;
  }

  if (mysql_ping(connection_) != 0) {
    set_error(error, mysql_error(connection_));
    return false;
  }

  return true;
}

MYSQL* MySqlClient::raw() const noexcept { return connection_; }

void MySqlClient::close() noexcept {
  if (connection_ != nullptr) {
    mysql_close(connection_);
    connection_ = nullptr;
  }
}

void MySqlClient::set_error(std::string* error,
                            const std::string& message) const {
  if (error != nullptr) {
    *error = message;
  }
}

}  // namespace oj::db

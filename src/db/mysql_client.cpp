#include "db/mysql_client.h"

#include <stdexcept>
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

bool MySqlClient::execute(const std::string& sql, std::string* error) {
  if (connection_ == nullptr) {
    set_error(error, "mysql connection is not open");
    return false;
  }

  if (mysql_query(connection_, sql.c_str()) != 0) {
    set_error(error, mysql_error(connection_));
    return false;
  }

  MYSQL_RES* result = mysql_store_result(connection_);
  if (result != nullptr) {
    mysql_free_result(result);
  } else if (mysql_field_count(connection_) != 0) {
    set_error(error, mysql_error(connection_));
    return false;
  }

  return true;
}

bool MySqlClient::query(const std::string& sql, QueryResult* result,
                        std::string* error) {
  if (result != nullptr) {
    *result = QueryResult{};
  }

  if (connection_ == nullptr) {
    set_error(error, "mysql connection is not open");
    return false;
  }

  if (mysql_query(connection_, sql.c_str()) != 0) {
    set_error(error, mysql_error(connection_));
    return false;
  }

  MYSQL_RES* raw_result = mysql_store_result(connection_);
  if (raw_result == nullptr) {
    if (mysql_field_count(connection_) != 0) {
      set_error(error, mysql_error(connection_));
      return false;
    }

    if (result != nullptr) {
      result->affected_rows = mysql_affected_rows(connection_);
      result->insert_id = mysql_insert_id(connection_);
    }
    return true;
  }

  const unsigned int field_count = mysql_num_fields(raw_result);
  MYSQL_FIELD* fields = mysql_fetch_fields(raw_result);

  QueryResult output;
  output.columns.reserve(field_count);
  for (unsigned int i = 0; i < field_count; ++i) {
    output.columns.emplace_back(fields[i].name == nullptr ? "" : fields[i].name);
  }

  MYSQL_ROW row = nullptr;
  while ((row = mysql_fetch_row(raw_result)) != nullptr) {
    const unsigned long* lengths = mysql_fetch_lengths(raw_result);
    QueryResult::Row output_row;
    for (unsigned int i = 0; i < field_count; ++i) {
      if (row[i] == nullptr) {
        output_row[output.columns[i]] = std::nullopt;
      } else {
        output_row[output.columns[i]] =
            std::string(row[i], static_cast<std::size_t>(lengths[i]));
      }
    }
    output.rows.push_back(std::move(output_row));
  }

  output.affected_rows = mysql_affected_rows(connection_);
  output.insert_id = mysql_insert_id(connection_);
  mysql_free_result(raw_result);

  if (result != nullptr) {
    *result = std::move(output);
  }
  return true;
}

std::string MySqlClient::escape(std::string_view value) const {
  if (connection_ == nullptr) {
    throw std::runtime_error("mysql connection is not open");
  }

  std::string escaped(value.size() * 2 + 1, '\0');
  const unsigned long length =
      mysql_real_escape_string(connection_, escaped.data(), value.data(),
                               static_cast<unsigned long>(value.size()));
  escaped.resize(length);
  return escaped;
}

bool MySqlClient::is_connected() const noexcept { return connection_ != nullptr; }

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

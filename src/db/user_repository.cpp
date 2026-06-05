#include "db/user_repository.h"

#include <cstdlib>
#include <stdexcept>
#include <utility>

namespace oj::db {
namespace {

std::string row_value(const QueryResult::Row& row, const std::string& key) {
  const auto it = row.find(key);
  if (it == row.end() || !it->second.has_value()) {
    return "";
  }
  return *it->second;
}

std::uint64_t parse_uint64(const std::string& value) {
  return static_cast<std::uint64_t>(std::stoull(value));
}

}  // namespace

UserRepository::UserRepository(MySqlClient& client) : client_(client) {}

bool UserRepository::find_by_username(const std::string& username,
                                      std::optional<model::User>* user,
                                      std::string* error) {
  if (user != nullptr) {
    *user = std::nullopt;
  }

  const std::string sql =
      "SELECT id, username, password_hash FROM users WHERE username = '" +
      client_.escape(username) + "' LIMIT 1";

  QueryResult result;
  if (!client_.query(sql, &result, error)) {
    return false;
  }

  if (result.rows.empty()) {
    return true;
  }

  const auto& row = result.rows.front();
  model::User output;
  output.id = parse_uint64(row_value(row, "id"));
  output.username = row_value(row, "username");
  output.password_hash = row_value(row, "password_hash");

  if (user != nullptr) {
    *user = std::move(output);
  }
  return true;
}

bool UserRepository::create(const std::string& username,
                            const std::string& password_hash,
                            std::uint64_t* id, std::string* error) {
  const std::string sql =
      "INSERT INTO users (username, password_hash) VALUES ('" +
      client_.escape(username) + "', '" + client_.escape(password_hash) + "')";

  QueryResult result;
  if (!client_.query(sql, &result, error)) {
    return false;
  }

  if (id != nullptr) {
    *id = result.insert_id;
  }
  return true;
}

}  // namespace oj::db

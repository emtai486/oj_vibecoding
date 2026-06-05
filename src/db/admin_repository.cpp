#include "db/admin_repository.h"

#include <cstdlib>
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

AdminRepository::AdminRepository(MySqlClient& client) : client_(client) {}

bool AdminRepository::find_by_username(const std::string& username,
                                       std::optional<model::User>* admin,
                                       std::string* error) {
  if (admin != nullptr) {
    *admin = std::nullopt;
  }

  const std::string sql =
      "SELECT id, username, password_hash FROM admins WHERE username = '" +
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

  if (admin != nullptr) {
    *admin = std::move(output);
  }
  return true;
}

}  // namespace oj::db

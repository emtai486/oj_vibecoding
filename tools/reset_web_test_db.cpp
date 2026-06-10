#include "config/config.h"
#include "db/mysql_client.h"

#include <exception>
#include <iostream>
#include <stdexcept>
#include <string>
#include <vector>

namespace {

constexpr const char* kUserPasswordHash =
    "pbkdf2_sha256$120000$oj-user1-v1$"
    "559a3423ecafda16f42a4b30389959d58e637b750cbb65eb77edb31333bdf2c4";
constexpr const char* kAdminPasswordHash =
    "pbkdf2_sha256$120000$oj-admin-v1$"
    "0202a5c043297dd8cf8f64563a05224a6956701381d6ff27f1ebfd5746613b78";

void print_usage(const char* program) {
  std::cerr
      << "Usage:\n"
      << "  " << program << " [config_path] --yes\n\n"
      << "Reset the configured MySQL database to the Web/API automation "
         "baseline data.\n\n"
      << "This deletes rows from users, admins, problems, and testcases, then "
         "recreates:\n"
      << "  user1 / password\n"
      << "  admin / password\n"
      << "  problem 1: A+B Problem\n"
      << "  problem 2: Average Score\n";
}

std::string quote(oj::db::MySqlClient& client, const std::string& value) {
  return "'" + client.escape(value) + "'";
}

void execute_or_throw(oj::db::MySqlClient& client, const std::string& sql) {
  std::string error;
  if (!client.execute(sql, &error)) {
    throw std::runtime_error("SQL failed: " + error + "\nSQL: " + sql);
  }
}

void query_or_throw(oj::db::MySqlClient& client, const std::string& sql,
                    oj::db::QueryResult* result) {
  std::string error;
  if (!client.query(sql, result, &error)) {
    throw std::runtime_error("SQL failed: " + error + "\nSQL: " + sql);
  }
}

void ensure_required_tables(oj::db::MySqlClient& client) {
  const std::vector<std::string> tables = {"users", "admins", "problems",
                                           "testcases"};
  for (const auto& table : tables) {
    oj::db::QueryResult result;
    query_or_throw(client, "SHOW TABLES LIKE " + quote(client, table), &result);
    if (result.rows.empty()) {
      throw std::runtime_error("required table missing: " + table +
                               "; run scripts/init_db.sh config/app.conf first");
    }
  }
}

void insert_user_data(oj::db::MySqlClient& client) {
  execute_or_throw(
      client,
      "INSERT INTO users (username, password_hash) VALUES (" +
          quote(client, "user1") + ", " + quote(client, kUserPasswordHash) +
          ")");

  execute_or_throw(
      client,
      "INSERT INTO admins (username, password_hash) VALUES (" +
          quote(client, "admin") + ", " + quote(client, kAdminPasswordHash) +
          ")");
}

void insert_problem_data(oj::db::MySqlClient& client) {
  execute_or_throw(
      client,
      "INSERT INTO problems (id, title, difficulty, description, input_format, "
      "output_format, sample_input, sample_output, time_limit_ms, "
      "memory_limit_kb, compare_mode) VALUES "
      "(1, " +
          quote(client, "A+B Problem") + ", " + quote(client, "easy") + ", " +
          quote(client, "Read two integers a and b and output their sum.") +
          ", " + quote(client, "Two integers a and b separated by spaces.") +
          ", " + quote(client, "Print one integer: a + b.") + ", " +
          quote(client, "1 2\n") + ", " + quote(client, "3\n") +
          ", 1000, 131072, " + quote(client, "strict") + "), "
      "(2, " +
          quote(client, "Average Score") + ", " + quote(client, "easy") +
          ", " +
          quote(client,
                "Read n scores and print their average rounded to one "
                "decimal place.") +
          ", " +
          quote(client,
                "The first line contains n. The second line contains n "
                "integer scores.") +
          ", " +
          quote(client,
                "Print the average score with one digit after the decimal "
                "point.") +
          ", " + quote(client, "3\n1 2 4\n") + ", " +
          quote(client, "2.3\n") + ", 1000, 131072, " +
          quote(client, "float_1") + ")");

  execute_or_throw(
      client,
      "INSERT INTO testcases (id, problem_id, `input`, expected_output, "
      "is_sample) VALUES "
      "(1, 1, " +
          quote(client, "1 2\n") + ", " + quote(client, "3\n") +
          ", TRUE), "
      "(2, 1, " +
          quote(client, "10 20\n") + ", " + quote(client, "30\n") +
          ", FALSE), "
      "(3, 1, " +
          quote(client, "-5 8\n") + ", " + quote(client, "3\n") +
          ", FALSE), "
      "(4, 2, " +
          quote(client, "3\n1 2 4\n") + ", " + quote(client, "2.3\n") +
          ", TRUE), "
      "(5, 2, " +
          quote(client, "4\n80 90 100 70\n") + ", " +
          quote(client, "85.0\n") + ", FALSE), "
      "(6, 2, " +
          quote(client, "1\n5\n") + ", " + quote(client, "5.0\n") +
          ", FALSE)");
}

void print_count(oj::db::MySqlClient& client, const std::string& table) {
  oj::db::QueryResult result;
  query_or_throw(client, "SELECT COUNT(*) AS count FROM " + table, &result);
  std::string count = "0";
  if (!result.rows.empty()) {
    const auto it = result.rows.front().find("count");
    if (it != result.rows.front().end() && it->second.has_value()) {
      count = *it->second;
    }
  }
  std::cout << "  " << table << ": " << count << '\n';
}

}  // namespace

int main(int argc, char* argv[]) {
  std::string config_path = "config/app.conf";
  bool confirmed = false;

  for (int i = 1; i < argc; ++i) {
    const std::string arg = argv[i];
    if (arg == "--yes") {
      confirmed = true;
    } else if (arg == "--help" || arg == "-h") {
      print_usage(argv[0]);
      return 0;
    } else if (config_path == "config/app.conf") {
      config_path = arg;
    } else {
      std::cerr << "unexpected argument: " << arg << '\n';
      print_usage(argv[0]);
      return 2;
    }
  }

  if (!confirmed) {
    std::cerr << "refusing to reset database without --yes\n";
    print_usage(argv[0]);
    return 2;
  }

  try {
    const auto config = oj::config::load_config(config_path);
    oj::db::MySqlClient client(config.mysql);
    std::string error;
    if (!client.connect(&error)) {
      std::cerr << "failed to connect database: " << error << '\n';
      return 1;
    }

    ensure_required_tables(client);

    execute_or_throw(client, "START TRANSACTION");
    try {
      execute_or_throw(client, "DELETE FROM testcases");
      execute_or_throw(client, "DELETE FROM problems");
      execute_or_throw(client, "DELETE FROM users");
      execute_or_throw(client, "DELETE FROM admins");
      insert_user_data(client);
      insert_problem_data(client);
      execute_or_throw(client, "COMMIT");
    } catch (...) {
      try {
        execute_or_throw(client, "ROLLBACK");
      } catch (const std::exception& rollback_error) {
        std::cerr << "rollback failed: " << rollback_error.what() << '\n';
      }
      throw;
    }

    execute_or_throw(client, "ALTER TABLE users AUTO_INCREMENT = 2");
    execute_or_throw(client, "ALTER TABLE admins AUTO_INCREMENT = 2");
    execute_or_throw(client, "ALTER TABLE problems AUTO_INCREMENT = 3");
    execute_or_throw(client, "ALTER TABLE testcases AUTO_INCREMENT = 7");

    std::cout << "PASS: reset database to Web/API automation baseline\n";
    std::cout << "Database: " << config.mysql.host << ':' << config.mysql.port
              << '/' << config.mysql.database << '\n';
    std::cout << "Rows after reset:\n";
    print_count(client, "users");
    print_count(client, "admins");
    print_count(client, "problems");
    print_count(client, "testcases");
    std::cout << "Accounts:\n";
    std::cout << "  user1 / password\n";
    std::cout << "  admin / password\n";
    std::cout << "Problems:\n";
    std::cout << "  1: A+B Problem\n";
    std::cout << "  2: Average Score\n";
  } catch (const std::exception& ex) {
    std::cerr << "FAIL: " << ex.what() << '\n';
    return 1;
  }

  return 0;
}

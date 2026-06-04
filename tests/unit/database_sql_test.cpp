#include <algorithm>
#include <cctype>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <sstream>
#include <stdexcept>
#include <string>

namespace {

namespace fs = std::filesystem;

int failures = 0;

void expect(bool condition, const std::string& message) {
  if (!condition) {
    std::cerr << "FAIL: " << message << '\n';
    ++failures;
  }
}

std::string read_text(const fs::path& path) {
  std::ifstream input(path);
  if (!input) {
    throw std::runtime_error("failed to read " + path.string());
  }

  std::ostringstream output;
  output << input.rdbuf();
  return output.str();
}

std::string lower_copy(std::string value) {
  std::transform(value.begin(), value.end(), value.begin(),
                 [](unsigned char ch) {
                   return static_cast<char>(std::tolower(ch));
                 });
  return value;
}

std::string normalize_sql(std::string value) {
  value = lower_copy(std::move(value));
  for (char& ch : value) {
    if (std::isspace(static_cast<unsigned char>(ch)) != 0) {
      ch = ' ';
    }
  }

  std::string normalized;
  normalized.reserve(value.size());
  bool previous_space = false;
  for (const char ch : value) {
    if (ch == ' ') {
      if (!previous_space) {
        normalized.push_back(ch);
      }
      previous_space = true;
      continue;
    }

    normalized.push_back(ch);
    previous_space = false;
  }

  return normalized;
}

void expect_contains(const std::string& text, const std::string& needle,
                     const std::string& message) {
  expect(text.find(needle) != std::string::npos, message);
}

std::string table_section(const std::string& schema, const std::string& table,
                          const std::string& next_table = "") {
  const std::string marker = "create table if not exists " + table;
  const auto start = schema.find(marker);
  if (start == std::string::npos) {
    return "";
  }

  if (next_table.empty()) {
    return schema.substr(start);
  }

  const std::string next_marker = "create table if not exists " + next_table;
  const auto end = schema.find(next_marker, start + marker.size());
  if (end == std::string::npos) {
    return schema.substr(start);
  }

  return schema.substr(start, end - start);
}

void test_schema_tables(const std::string& schema) {
  const std::string users = table_section(schema, "users", "admins");
  const std::string admins = table_section(schema, "admins", "problems");
  const std::string problems = table_section(schema, "problems", "testcases");
  const std::string testcases = table_section(schema, "testcases");

  expect(!users.empty(), "schema should create users table");
  expect(!admins.empty(), "schema should create admins table");
  expect(!problems.empty(), "schema should create problems table");
  expect(!testcases.empty(), "schema should create testcases table");

  expect_contains(users, "id bigint unsigned not null auto_increment",
                  "users.id should be auto incrementing");
  expect_contains(users, "username varchar(64) not null",
                  "users.username should exist");
  expect_contains(users, "password_hash varchar(255) not null",
                  "users.password_hash should exist");
  expect_contains(users, "created_at timestamp not null default current_timestamp",
                  "users.created_at should default to current timestamp");
  expect_contains(users, "unique key uq_users_username (username)",
                  "users.username should be unique");

  expect_contains(admins, "id bigint unsigned not null auto_increment",
                  "admins.id should be auto incrementing");
  expect_contains(admins, "username varchar(64) not null",
                  "admins.username should exist");
  expect_contains(admins, "password_hash varchar(255) not null",
                  "admins.password_hash should exist");
  expect_contains(admins, "created_at timestamp not null default current_timestamp",
                  "admins.created_at should default to current timestamp");
  expect_contains(admins, "unique key uq_admins_username (username)",
                  "admins.username should be unique");

  expect_contains(problems, "title varchar(200) not null",
                  "problems.title should exist");
  expect_contains(problems, "difficulty varchar(16) not null default 'easy'",
                  "problems.difficulty should default to easy");
  expect_contains(problems, "description text not null",
                  "problems.description should exist");
  expect_contains(problems, "input_format text not null",
                  "problems.input_format should exist");
  expect_contains(problems, "output_format text not null",
                  "problems.output_format should exist");
  expect_contains(problems, "sample_input text not null",
                  "problems.sample_input should exist");
  expect_contains(problems, "sample_output text not null",
                  "problems.sample_output should exist");
  expect_contains(problems, "time_limit_ms int unsigned not null default 1000",
                  "problems.time_limit_ms should default to 1000");
  expect_contains(problems,
                  "memory_limit_kb int unsigned not null default 131072",
                  "problems.memory_limit_kb should default to 128 MB");
  expect_contains(problems, "compare_mode varchar(16) not null default 'strict'",
                  "problems.compare_mode should default to strict");
  expect_contains(problems, "check (difficulty in ('easy', 'medium', 'hard'))",
                  "problems.difficulty should be constrained");
  expect_contains(problems, "check (compare_mode in ('strict', 'float_1'))",
                  "problems.compare_mode should be constrained");

  expect_contains(testcases, "problem_id bigint unsigned not null",
                  "testcases.problem_id should exist");
  expect_contains(testcases, "`input` text not null",
                  "testcases.input should exist");
  expect_contains(testcases, "expected_output text not null",
                  "testcases.expected_output should exist");
  expect_contains(testcases, "is_sample boolean not null default false",
                  "testcases.is_sample should default to false");
  expect_contains(testcases, "foreign key (problem_id) references problems (id)",
                  "testcases should reference problems");
  expect_contains(testcases, "on delete cascade",
                  "deleting a problem should delete its testcases");
}

void test_seed_data(const std::string& seed) {
  expect_contains(seed, "start transaction;",
                  "seed data should run in a transaction");
  expect_contains(seed, "insert into users (username, password_hash)",
                  "seed should insert a normal user");
  expect_contains(seed, "'user1'", "seed should include user1 account");
  expect_contains(seed, "pbkdf2_sha256$120000$oj-user1-v1$",
                  "user1 password should be stored as a PBKDF2 hash");
  expect_contains(seed, "insert into admins (username, password_hash)",
                  "seed should insert an admin");
  expect_contains(seed, "'admin'", "seed should include admin account");
  expect_contains(seed, "pbkdf2_sha256$120000$oj-admin-v1$",
                  "admin password should be stored as a PBKDF2 hash");
  expect_contains(seed, "on duplicate key update",
                  "seed should be safe to run repeatedly");

  expect_contains(seed, "insert into problems",
                  "seed should insert example problems");
  expect_contains(seed, "'a+b problem'",
                  "seed should include an A+B example problem");
  expect_contains(seed, "'average score'",
                  "seed should include a float comparison example problem");
  expect_contains(seed, "'strict'", "seed should include a strict problem");
  expect_contains(seed, "'float_1'",
                  "seed should include a float_1 problem");

  expect_contains(seed, "insert into testcases",
                  "seed should insert example testcases");
  expect_contains(seed, "true", "seed should include sample testcases");
  expect_contains(seed, "false", "seed should include hidden testcases");
  expect_contains(seed, "commit;", "seed transaction should commit");
}

}  // namespace

int main(int argc, char* argv[]) {
  const fs::path root =
      argc >= 2 ? fs::absolute(argv[1]) : fs::current_path();

  try {
    const fs::path schema_path = root / "sql/schema.sql";
    const fs::path seed_path = root / "sql/seed.sql";

    expect(fs::is_regular_file(schema_path), "sql/schema.sql should exist");
    expect(fs::is_regular_file(seed_path), "sql/seed.sql should exist");

    const std::string schema = normalize_sql(read_text(schema_path));
    const std::string seed = normalize_sql(read_text(seed_path));

    test_schema_tables(schema);
    test_seed_data(seed);
  } catch (const std::exception& error) {
    std::cerr << "FAIL: unexpected exception: " << error.what() << '\n';
    return 1;
  }

  if (failures != 0) {
    std::cerr << failures << " database SQL test(s) failed\n";
    return 1;
  }

  std::cout << "PASS: SPEC 12.2 database SQL checks passed\n";
  return 0;
}

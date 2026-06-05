#include "db/testcase_repository.h"

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

bool parse_bool(const std::string& value) {
  return value == "1" || value == "true" || value == "TRUE";
}

}  // namespace

TestcaseRepository::TestcaseRepository(MySqlClient& client) : client_(client) {}

bool TestcaseRepository::list_for_problem(
    std::uint64_t problem_id, bool is_sample,
    std::vector<model::Testcase>* testcases, std::string* error) {
  if (testcases != nullptr) {
    testcases->clear();
  }

  QueryResult result;
  if (!client_.query(
          "SELECT id, problem_id, `input`, expected_output, is_sample "
          "FROM testcases WHERE problem_id = " +
              std::to_string(problem_id) + " AND is_sample = " +
              (is_sample ? "TRUE" : "FALSE") + " ORDER BY id ASC",
          &result, error)) {
    return false;
  }

  std::vector<model::Testcase> output;
  output.reserve(result.rows.size());
  for (const auto& row : result.rows) {
    model::Testcase testcase;
    testcase.id = parse_uint64(row_value(row, "id"));
    testcase.problem_id = parse_uint64(row_value(row, "problem_id"));
    testcase.input = row_value(row, "input");
    testcase.expected_output = row_value(row, "expected_output");
    testcase.is_sample = parse_bool(row_value(row, "is_sample"));
    output.push_back(std::move(testcase));
  }

  if (testcases != nullptr) {
    *testcases = std::move(output);
  }
  return true;
}

}  // namespace oj::db

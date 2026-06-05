#include "db/problem_repository.h"

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

std::uint32_t parse_uint32(const std::string& value) {
  return static_cast<std::uint32_t>(std::stoul(value));
}

std::uint64_t parse_uint64(const std::string& value) {
  return static_cast<std::uint64_t>(std::stoull(value));
}

model::Problem row_to_problem(const QueryResult::Row& row) {
  model::Problem problem;
  problem.id = parse_uint64(row_value(row, "id"));
  problem.title = row_value(row, "title");
  problem.difficulty = row_value(row, "difficulty");
  problem.description = row_value(row, "description");
  problem.input_format = row_value(row, "input_format");
  problem.output_format = row_value(row, "output_format");
  problem.sample_input = row_value(row, "sample_input");
  problem.sample_output = row_value(row, "sample_output");
  problem.time_limit_ms = parse_uint32(row_value(row, "time_limit_ms"));
  problem.memory_limit_kb = parse_uint32(row_value(row, "memory_limit_kb"));
  problem.compare_mode = row_value(row, "compare_mode");
  return problem;
}

}  // namespace

ProblemRepository::ProblemRepository(MySqlClient& client) : client_(client) {}

bool ProblemRepository::list(std::vector<model::ProblemSummary>* problems,
                             std::string* error) {
  if (problems != nullptr) {
    problems->clear();
  }

  QueryResult result;
  if (!client_.query(
          "SELECT id, title, difficulty FROM problems ORDER BY id ASC",
          &result, error)) {
    return false;
  }

  std::vector<model::ProblemSummary> output;
  output.reserve(result.rows.size());
  for (const auto& row : result.rows) {
    model::ProblemSummary problem;
    problem.id = parse_uint64(row_value(row, "id"));
    problem.title = row_value(row, "title");
    problem.difficulty = row_value(row, "difficulty");
    output.push_back(std::move(problem));
  }

  if (problems != nullptr) {
    *problems = std::move(output);
  }
  return true;
}

bool ProblemRepository::find_by_id(std::uint64_t id,
                                   std::optional<model::Problem>* problem,
                                   std::string* error) {
  if (problem != nullptr) {
    *problem = std::nullopt;
  }

  QueryResult result;
  if (!client_.query(
          "SELECT id, title, difficulty, description, input_format, "
          "output_format, sample_input, sample_output, time_limit_ms, "
          "memory_limit_kb, compare_mode FROM problems WHERE id = " +
              std::to_string(id) + " LIMIT 1",
          &result, error)) {
    return false;
  }

  if (result.rows.empty()) {
    return true;
  }

  if (problem != nullptr) {
    *problem = row_to_problem(result.rows.front());
  }
  return true;
}

bool ProblemRepository::create_with_testcases(
    const model::Problem& problem, const std::vector<model::Testcase>& testcases,
    std::uint64_t* id, std::string* error) {
  if (!client_.execute("START TRANSACTION", error)) {
    return false;
  }

  const std::string insert_problem =
      "INSERT INTO problems (title, difficulty, description, input_format, "
      "output_format, sample_input, sample_output, time_limit_ms, "
      "memory_limit_kb, compare_mode) VALUES ('" +
      client_.escape(problem.title) + "', '" +
      client_.escape(problem.difficulty) + "', '" +
      client_.escape(problem.description) + "', '" +
      client_.escape(problem.input_format) + "', '" +
      client_.escape(problem.output_format) + "', '" +
      client_.escape(problem.sample_input) + "', '" +
      client_.escape(problem.sample_output) + "', " +
      std::to_string(problem.time_limit_ms) + ", " +
      std::to_string(problem.memory_limit_kb) + ", '" +
      client_.escape(problem.compare_mode) + "')";

  QueryResult result;
  if (!client_.query(insert_problem, &result, error)) {
    (void)client_.execute("ROLLBACK", nullptr);
    return false;
  }

  const std::uint64_t problem_id = result.insert_id;
  for (const auto& testcase : testcases) {
    const std::string insert_testcase =
        "INSERT INTO testcases (problem_id, `input`, expected_output, "
        "is_sample) VALUES (" +
        std::to_string(problem_id) + ", '" + client_.escape(testcase.input) +
        "', '" + client_.escape(testcase.expected_output) + "', " +
        (testcase.is_sample ? "TRUE" : "FALSE") + ")";
    if (!client_.execute(insert_testcase, error)) {
      (void)client_.execute("ROLLBACK", nullptr);
      return false;
    }
  }

  if (!client_.execute("COMMIT", error)) {
    (void)client_.execute("ROLLBACK", nullptr);
    return false;
  }

  if (id != nullptr) {
    *id = problem_id;
  }
  return true;
}

bool ProblemRepository::delete_by_id(std::uint64_t id, bool* deleted,
                                     std::string* error) {
  if (deleted != nullptr) {
    *deleted = false;
  }

  QueryResult result;
  if (!client_.query("DELETE FROM problems WHERE id = " + std::to_string(id),
                     &result, error)) {
    return false;
  }

  if (deleted != nullptr) {
    *deleted = result.affected_rows > 0;
  }
  return true;
}

}  // namespace oj::db

#include "api/problem_api.h"

#include "api/database_helpers.h"
#include "db/problem_repository.h"
#include "db/testcase_repository.h"
#include "model/problem.h"
#include "model/testcase.h"
#include "util/json_response.h"

#include <cstdint>
#include <optional>
#include <string>
#include <utility>
#include <vector>

namespace oj::api {
namespace {

oj::util::json::Value problem_summary_json(
    const model::ProblemSummary& problem) {
  return oj::util::json::Value::Object{
      {"id", static_cast<std::int64_t>(problem.id)},
      {"title", problem.title},
      {"difficulty", problem.difficulty},
  };
}

oj::util::json::Value testcase_json(const model::Testcase& testcase) {
  return oj::util::json::Value::Object{
      {"id", static_cast<std::int64_t>(testcase.id)},
      {"input", testcase.input},
      {"expected_output", testcase.expected_output},
      {"is_sample", testcase.is_sample},
  };
}

oj::util::json::Value problem_json(
    const model::Problem& problem,
    const std::vector<model::Testcase>& sample_testcases) {
  oj::util::json::Value::Array samples;
  samples.reserve(sample_testcases.size());
  for (const auto& testcase : sample_testcases) {
    samples.push_back(testcase_json(testcase));
  }

  return oj::util::json::Value::Object{
      {"id", static_cast<std::int64_t>(problem.id)},
      {"title", problem.title},
      {"difficulty", problem.difficulty},
      {"description", problem.description},
      {"input_format", problem.input_format},
      {"output_format", problem.output_format},
      {"sample_input", problem.sample_input},
      {"sample_output", problem.sample_output},
      {"time_limit_ms", static_cast<std::int64_t>(problem.time_limit_ms)},
      {"memory_limit_kb", static_cast<std::int64_t>(problem.memory_limit_kb)},
      {"compare_mode", problem.compare_mode},
      {"samples", std::move(samples)},
  };
}

}  // namespace

void register_problem_routes(httplib::Server& server,
                             std::shared_ptr<db::MySqlConnectionPool> mysql_pool) {
  server.Get("/api/problems", [mysql_pool](const httplib::Request& request,
                                           httplib::Response& response) {
    db::PooledMySqlClient client;
    if (!acquire_db(mysql_pool, request, response, &client)) {
      return;
    }

    db::ProblemRepository repository(*client);
    std::vector<model::ProblemSummary> problems;
    std::string error;
    if (!repository.list(&problems, &error)) {
      send_database_error(request, response, "list problems", error);
      return;
    }

    oj::util::json::Value::Array data;
    data.reserve(problems.size());
    for (const auto& problem : problems) {
      data.push_back(problem_summary_json(problem));
    }
    oj::util::send_success(response, oj::util::json::Value(std::move(data)));
  });

  server.Get(R"(/api/problems/(\d+))",
             [mysql_pool](const httplib::Request& request,
                          httplib::Response& response) {
               const auto id =
                   static_cast<std::uint64_t>(std::stoull(request.matches[1]));

               db::PooledMySqlClient client;
               if (!acquire_db(mysql_pool, request, response, &client)) {
                 return;
               }

               db::ProblemRepository problem_repository(*client);
               std::optional<model::Problem> problem;
               std::string error;
               if (!problem_repository.find_by_id(id, &problem, &error)) {
                 send_database_error(request, response, "find problem", error);
                 return;
               }

               if (!problem.has_value()) {
                 oj::util::send_error(response,
                                      httplib::StatusCode::NotFound_404,
                                      "not found");
                 return;
               }

               db::TestcaseRepository testcase_repository(*client);
               std::vector<model::Testcase> samples;
               if (!testcase_repository.list_for_problem(id, true, &samples,
                                                         &error)) {
                 send_database_error(request, response, "list sample testcases",
                                     error);
                 return;
               }

               oj::util::send_success(
                   response, problem_json(*problem, samples));
             });
}

}  // namespace oj::api

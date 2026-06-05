#include "api/submit_api.h"

#include "db/mysql_client.h"
#include "db/problem_repository.h"
#include "db/testcase_repository.h"
#include "judge/judge_service.h"
#include "util/json_response.h"

#include <cstdint>
#include <optional>
#include <string>
#include <vector>

namespace oj::api {
namespace {

bool connect_db(db::MySqlClient* client, httplib::Response& response) {
  std::string error;
  if (!client->connect(&error)) {
    oj::util::send_error(response,
                         httplib::StatusCode::InternalServerError_500,
                         "database error");
    return false;
  }
  return true;
}

const oj::util::json::Value* object_field(const oj::util::json::Value& body,
                                          const std::string& key) {
  if (!body.is_object()) {
    return nullptr;
  }
  const auto& object = body.as_object();
  const auto it = object.find(key);
  if (it == object.end()) {
    return nullptr;
  }
  return &it->second;
}

std::optional<std::uint64_t> uint_field(const oj::util::json::Value& body,
                                        const std::string& key) {
  const auto* value = object_field(body, key);
  if (value == nullptr || !value->is_number() || value->as_int() <= 0) {
    return std::nullopt;
  }
  return static_cast<std::uint64_t>(value->as_int());
}

std::optional<std::string> string_field(const oj::util::json::Value& body,
                                        const std::string& key) {
  const auto* value = object_field(body, key);
  if (value == nullptr || !value->is_string()) {
    return std::nullopt;
  }
  return value->as_string();
}

std::optional<auth::SessionUser> current_user(
    const httplib::Request& request,
    const std::shared_ptr<auth::SessionStore>& sessions) {
  const auto session_id = auth::cookie_value(request, "oj_user_session");
  if (!session_id.has_value()) {
    return std::nullopt;
  }
  return sessions->find_user_session(*session_id);
}

oj::util::json::Value submit_result_json(const std::string& result) {
  return oj::util::json::Value::Object{{"result", result}};
}

}  // namespace

void register_submit_routes(httplib::Server& server,
                            config::MySqlConfig mysql_config,
                            std::shared_ptr<auth::SessionStore> sessions) {
  server.Post("/api/submit",
              [mysql_config, sessions](const httplib::Request& request,
                                       httplib::Response& response) {
                if (!current_user(request, sessions).has_value()) {
                  oj::util::send_error(response,
                                       httplib::StatusCode::Unauthorized_401,
                                       "unauthorized");
                  return;
                }

                oj::util::json::Value body;
                if (!oj::util::parse_json_body(request, &body, response)) {
                  return;
                }

                const auto problem_id = uint_field(body, "problem_id");
                const auto code = string_field(body, "code");
                if (!problem_id.has_value() || !code.has_value() ||
                    code->empty()) {
                  oj::util::send_success(response,
                                         submit_result_json("failed"),
                                         "failed");
                  return;
                }

                db::MySqlClient client(mysql_config);
                if (!connect_db(&client, response)) {
                  return;
                }

                db::ProblemRepository problem_repository(client);
                std::optional<model::Problem> problem;
                std::string error;
                if (!problem_repository.find_by_id(*problem_id, &problem,
                                                   &error)) {
                  oj::util::send_error(
                      response,
                      httplib::StatusCode::InternalServerError_500,
                      "database error");
                  return;
                }
                if (!problem.has_value()) {
                  oj::util::send_success(response,
                                         submit_result_json("failed"),
                                         "failed");
                  return;
                }

                db::TestcaseRepository testcase_repository(client);
                std::vector<model::Testcase> hidden_testcases;
                if (!testcase_repository.list_for_problem(*problem_id, false,
                                                          &hidden_testcases,
                                                          &error)) {
                  oj::util::send_error(
                      response,
                      httplib::StatusCode::InternalServerError_500,
                      "database error");
                  return;
                }

                judge::JudgeService judge_service;
                const auto result =
                    judge_service.judge(*problem, hidden_testcases, *code);
                if (result == judge::JudgeResult::Passed) {
                  oj::util::send_success(response,
                                         submit_result_json("passed"),
                                         "accepted");
                  return;
                }

                oj::util::send_success(response, submit_result_json("failed"),
                                       "failed");
              });
}

}  // namespace oj::api

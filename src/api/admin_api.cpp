#include "api/admin_api.h"

#include "api/database_helpers.h"
#include "auth/password.h"
#include "db/admin_repository.h"
#include "db/problem_repository.h"
#include "model/problem.h"
#include "model/testcase.h"
#include "util/json_response.h"

#include <algorithm>
#include <cstdint>
#include <optional>
#include <string>
#include <utility>
#include <vector>

namespace oj::api {
namespace {

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

std::optional<std::string> string_field(const oj::util::json::Value& body,
                                        const std::string& key) {
  const auto* value = object_field(body, key);
  if (value == nullptr || !value->is_string()) {
    return std::nullopt;
  }
  return value->as_string();
}

std::optional<std::uint32_t> uint32_field(const oj::util::json::Value& body,
                                          const std::string& key) {
  const auto* value = object_field(body, key);
  if (value == nullptr || !value->is_int() || value->as_int() < 0) {
    return std::nullopt;
  }
  return static_cast<std::uint32_t>(value->as_int());
}

bool allowed_value(const std::string& value,
                   const std::vector<std::string>& allowed) {
  return std::find(allowed.begin(), allowed.end(), value) != allowed.end();
}

std::optional<auth::SessionAdmin> current_admin(
    const httplib::Request& request,
    const std::shared_ptr<auth::SessionStore>& sessions) {
  const auto session_id = auth::cookie_value(request, "oj_admin_session");
  if (!session_id.has_value()) {
    return std::nullopt;
  }
  return sessions->find_admin_session(*session_id);
}

oj::util::json::Value admin_json(std::uint64_t id,
                                 const std::string& username) {
  return oj::util::json::Value::Object{
      {"id", static_cast<std::int64_t>(id)},
      {"username", username},
  };
}

bool require_admin(const httplib::Request& request,
                   httplib::Response& response,
                   const std::shared_ptr<auth::SessionStore>& sessions) {
  if (current_admin(request, sessions).has_value()) {
    return true;
  }

  oj::util::send_error(response, httplib::StatusCode::Unauthorized_401,
                       "unauthorized");
  return false;
}

bool parse_testcase_array(const oj::util::json::Value& body,
                          const std::string& key, bool is_sample,
                          std::vector<model::Testcase>* output) {
  const auto* value = object_field(body, key);
  if (value == nullptr || !value->is_array()) {
    return false;
  }

  for (const auto& item : value->as_array()) {
    if (!item.is_object()) {
      return false;
    }

    const auto input = string_field(item, "input");
    const auto expected_output = string_field(item, "expected_output");
    if (!input.has_value() || !expected_output.has_value() ||
        expected_output->empty()) {
      return false;
    }

    model::Testcase testcase;
    testcase.input = *input;
    testcase.expected_output = *expected_output;
    testcase.is_sample = is_sample;
    output->push_back(std::move(testcase));
  }

  return true;
}

bool parse_problem_payload(const oj::util::json::Value& body,
                           model::Problem* problem,
                           std::vector<model::Testcase>* testcases) {
  if (!body.is_object() || problem == nullptr || testcases == nullptr) {
    return false;
  }

  const auto title = string_field(body, "title");
  const auto difficulty = string_field(body, "difficulty");
  const auto description = string_field(body, "description");
  const auto input_format = string_field(body, "input_format");
  const auto output_format = string_field(body, "output_format");
  const auto sample_input = string_field(body, "sample_input");
  const auto sample_output = string_field(body, "sample_output");
  const auto compare_mode = string_field(body, "compare_mode");
  const auto time_limit_ms = uint32_field(body, "time_limit_ms");
  const auto memory_limit_kb = uint32_field(body, "memory_limit_kb");

  if (!title.has_value() || title->empty() || title->size() > 200 ||
      !difficulty.has_value() ||
      !allowed_value(*difficulty, {"easy", "medium", "hard"}) ||
      !description.has_value() || description->empty() ||
      !input_format.has_value() || !output_format.has_value() ||
      !sample_input.has_value() || !sample_output.has_value() ||
      sample_output->empty() || !compare_mode.has_value() ||
      !allowed_value(*compare_mode, {"strict", "float_1"}) ||
      !time_limit_ms.has_value() || *time_limit_ms < 1 ||
      *time_limit_ms > 60000 || !memory_limit_kb.has_value() ||
      *memory_limit_kb < 1024 || *memory_limit_kb > 1048576) {
    return false;
  }

  model::Problem parsed;
  parsed.title = *title;
  parsed.difficulty = *difficulty;
  parsed.description = *description;
  parsed.input_format = *input_format;
  parsed.output_format = *output_format;
  parsed.sample_input = *sample_input;
  parsed.sample_output = *sample_output;
  parsed.time_limit_ms = *time_limit_ms;
  parsed.memory_limit_kb = *memory_limit_kb;
  parsed.compare_mode = *compare_mode;

  std::vector<model::Testcase> parsed_testcases;
  if (!parse_testcase_array(body, "samples", true, &parsed_testcases) ||
      !parse_testcase_array(body, "hidden_testcases", false,
                            &parsed_testcases)) {
    return false;
  }

  const auto sample_count = std::count_if(
      parsed_testcases.begin(), parsed_testcases.end(),
      [](const model::Testcase& testcase) { return testcase.is_sample; });
  const auto hidden_count = std::count_if(
      parsed_testcases.begin(), parsed_testcases.end(),
      [](const model::Testcase& testcase) { return !testcase.is_sample; });
  if (sample_count == 0 || hidden_count == 0) {
    return false;
  }

  *problem = std::move(parsed);
  *testcases = std::move(parsed_testcases);
  return true;
}

oj::util::json::Value problem_created_json(std::uint64_t id,
                                           const model::Problem& problem) {
  return oj::util::json::Value::Object{
      {"id", static_cast<std::int64_t>(id)},
      {"title", problem.title},
      {"difficulty", problem.difficulty},
  };
}

}  // namespace

void register_admin_routes(httplib::Server& server,
                           std::shared_ptr<db::MySqlConnectionPool> mysql_pool,
                           std::shared_ptr<auth::SessionStore> sessions) {
  server.Get("/api/admin/me",
             [sessions](const httplib::Request& request,
                        httplib::Response& response) {
               const auto admin = current_admin(request, sessions);
               if (!admin.has_value()) {
                 oj::util::send_success(
                     response,
                     oj::util::json::Value::Object{{"logged_in", false}});
                 return;
               }

               oj::util::send_success(
                   response,
                   oj::util::json::Value::Object{
                       {"logged_in", true},
                       {"admin", admin_json(admin->id, admin->username)},
                   });
             });

  server.Post("/api/admin/login",
              [mysql_pool, sessions](const httplib::Request& request,
                                     httplib::Response& response) {
                oj::util::json::Value body;
                if (!oj::util::parse_json_body(request, &body, response)) {
                  return;
                }

                const auto username = string_field(body, "username");
                const auto password = string_field(body, "password");
                if (!username.has_value() || !password.has_value()) {
                  oj::util::send_error(response,
                                       httplib::StatusCode::BadRequest_400,
                                       "invalid username or password");
                  return;
                }

                db::PooledMySqlClient client;
                if (!acquire_db(mysql_pool, request, response, &client)) {
                  return;
                }

                db::AdminRepository repository(*client);
                std::optional<model::User> admin;
                std::string error;
                if (!repository.find_by_username(*username, &admin, &error)) {
                  send_database_error(request, response, "find admin", error);
                  return;
                }

                if (!admin.has_value() ||
                    !auth::verify_password(*password, admin->password_hash)) {
                  oj::util::send_error(response,
                                       httplib::StatusCode::Unauthorized_401,
                                       "invalid username or password");
                  return;
                }

                const std::string session_id =
                    sessions->create_admin_session(admin->id, admin->username);
                auth::set_admin_session_cookie(response, session_id);
                oj::util::send_success(response,
                                       admin_json(admin->id, admin->username),
                                       "logged in");
              });

  server.Post("/api/admin/logout",
              [sessions](const httplib::Request& request,
                         httplib::Response& response) {
                const auto session_id =
                    auth::cookie_value(request, "oj_admin_session");
                if (session_id.has_value()) {
                  sessions->destroy_admin_session(*session_id);
                }
                auth::clear_admin_session_cookie(response);
                oj::util::send_success(response, nullptr, "logged out");
              });

  server.Post("/api/admin/problems",
              [mysql_pool, sessions](const httplib::Request& request,
                                     httplib::Response& response) {
                if (!require_admin(request, response, sessions)) {
                  return;
                }

                oj::util::json::Value body;
                if (!oj::util::parse_json_body(request, &body, response)) {
                  return;
                }

                model::Problem problem;
                std::vector<model::Testcase> testcases;
                if (!parse_problem_payload(body, &problem, &testcases)) {
                  oj::util::send_error(response,
                                       httplib::StatusCode::BadRequest_400,
                                       "invalid problem");
                  return;
                }

                db::PooledMySqlClient client;
                if (!acquire_db(mysql_pool, request, response, &client)) {
                  return;
                }

                db::ProblemRepository repository(*client);
                std::uint64_t id = 0;
                std::string error;
                if (!repository.create_with_testcases(problem, testcases, &id,
                                                      &error)) {
                  send_database_error(request, response, "create problem", error);
                  return;
                }

                oj::util::send_success(response,
                                       problem_created_json(id, problem),
                                       "created",
                                       httplib::StatusCode::Created_201);
              });

  server.Delete(R"(/api/admin/problems/(\d+))",
                [mysql_pool, sessions](const httplib::Request& request,
                                       httplib::Response& response) {
                  if (!require_admin(request, response, sessions)) {
                    return;
                  }

                  const auto id = static_cast<std::uint64_t>(
                      std::stoull(request.matches[1]));
                  db::PooledMySqlClient client;
                  if (!acquire_db(mysql_pool, request, response, &client)) {
                    return;
                  }

                  db::ProblemRepository repository(*client);
                  bool deleted = false;
                  std::string error;
                  if (!repository.delete_by_id(id, &deleted, &error)) {
                    send_database_error(request, response, "delete problem",
                                        error);
                    return;
                  }

                  if (!deleted) {
                    oj::util::send_error(response,
                                         httplib::StatusCode::NotFound_404,
                                         "not found");
                    return;
                  }

                  oj::util::send_success(
                      response,
                      oj::util::json::Value::Object{{"deleted", true}},
                      "deleted");
                });
}

}  // namespace oj::api

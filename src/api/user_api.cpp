#include "api/user_api.h"

#include "auth/password.h"
#include "db/mysql_client.h"
#include "db/user_repository.h"
#include "util/json_response.h"

#include <algorithm>
#include <cctype>
#include <optional>
#include <string>
#include <utility>

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

std::optional<std::string> string_field(const oj::util::json::Value& body,
                                        const std::string& key) {
  const auto* value = object_field(body, key);
  if (value == nullptr || !value->is_string()) {
    return std::nullopt;
  }
  return value->as_string();
}

bool valid_username(const std::string& username) {
  if (username.size() < 3 || username.size() > 64) {
    return false;
  }
  return std::all_of(username.begin(), username.end(), [](unsigned char ch) {
    return std::isalnum(ch) != 0 || ch == '_' || ch == '-';
  });
}

oj::util::json::Value user_json(std::uint64_t id,
                                const std::string& username) {
  return oj::util::json::Value::Object{
      {"id", static_cast<std::int64_t>(id)},
      {"username", username},
  };
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

}  // namespace

void register_user_routes(httplib::Server& server,
                          config::MySqlConfig mysql_config,
                          std::shared_ptr<auth::SessionStore> sessions) {
  server.Get("/api/user/me",
             [sessions](const httplib::Request& request,
                        httplib::Response& response) {
               const auto user = current_user(request, sessions);
               if (!user.has_value()) {
                 oj::util::send_success(
                     response,
                     oj::util::json::Value::Object{{"logged_in", false}});
                 return;
               }

               oj::util::send_success(
                   response, oj::util::json::Value::Object{
                                 {"logged_in", true},
                                 {"user", user_json(user->id, user->username)},
                             });
             });

  server.Post("/api/user/register",
              [mysql_config](const httplib::Request& request,
                             httplib::Response& response) {
                oj::util::json::Value body;
                if (!oj::util::parse_json_body(request, &body, response)) {
                  return;
                }

                const auto username = string_field(body, "username");
                const auto password = string_field(body, "password");
                if (!username.has_value() || !password.has_value() ||
                    !valid_username(*username) || password->size() < 6) {
                  oj::util::send_error(
                      response, httplib::StatusCode::BadRequest_400,
                      "invalid username or password");
                  return;
                }

                db::MySqlClient client(mysql_config);
                if (!connect_db(&client, response)) {
                  return;
                }

                db::UserRepository repository(client);
                std::optional<model::User> existing;
                std::string error;
                if (!repository.find_by_username(*username, &existing,
                                                 &error)) {
                  oj::util::send_error(
                      response,
                      httplib::StatusCode::InternalServerError_500,
                      "database error");
                  return;
                }
                if (existing.has_value()) {
                  oj::util::send_error(response,
                                       httplib::StatusCode::Conflict_409,
                                       "username exists");
                  return;
                }

                std::uint64_t id = 0;
                if (!repository.create(*username,
                                       auth::hash_password(*password), &id,
                                       &error)) {
                  oj::util::send_error(
                      response,
                      httplib::StatusCode::InternalServerError_500,
                      "database error");
                  return;
                }

                oj::util::send_success(response, user_json(id, *username),
                                       "registered",
                                       httplib::StatusCode::Created_201);
              });

  server.Post("/api/user/login",
              [mysql_config, sessions](const httplib::Request& request,
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

                db::MySqlClient client(mysql_config);
                if (!connect_db(&client, response)) {
                  return;
                }

                db::UserRepository repository(client);
                std::optional<model::User> user;
                std::string error;
                if (!repository.find_by_username(*username, &user, &error)) {
                  oj::util::send_error(
                      response,
                      httplib::StatusCode::InternalServerError_500,
                      "database error");
                  return;
                }

                if (!user.has_value() ||
                    !auth::verify_password(*password, user->password_hash)) {
                  oj::util::send_error(response,
                                       httplib::StatusCode::Unauthorized_401,
                                       "invalid username or password");
                  return;
                }

                const std::string session_id =
                    sessions->create_user_session(user->id, user->username);
                auth::set_user_session_cookie(response, session_id);
                oj::util::send_success(response,
                                       user_json(user->id, user->username),
                                       "logged in");
              });

  server.Post("/api/user/logout",
              [sessions](const httplib::Request& request,
                         httplib::Response& response) {
                const auto session_id =
                    auth::cookie_value(request, "oj_user_session");
                if (session_id.has_value()) {
                  sessions->destroy_user_session(*session_id);
                }
                auth::clear_user_session_cookie(response);
                oj::util::send_success(response, nullptr, "logged out");
              });
}

}  // namespace oj::api

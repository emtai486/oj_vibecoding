#include "util/json_response.h"

#include <utility>

namespace oj::util {

json::Value json_envelope(bool success, std::string message, json::Value data) {
  return json::Value::Object{
      {"success", json::Value(success)},
      {"message", json::Value(std::move(message))},
      {"data", std::move(data)},
  };
}

void send_json(httplib::Response& response, int status, const json::Value& body) {
  response.status = status;
  response.set_content(json::stringify(body),
                       "application/json; charset=utf-8");
}

void send_success(httplib::Response& response, json::Value data,
                  std::string message, int status) {
  send_json(response, status,
            json_envelope(true, std::move(message), std::move(data)));
}

void send_error(httplib::Response& response, int status, std::string message,
                json::Value data) {
  send_json(response, status,
            json_envelope(false, std::move(message), std::move(data)));
}

bool parse_json_body(const httplib::Request& request, json::Value* body,
                     httplib::Response& response) {
  std::string error;
  auto parsed = json::parse(request.body, &error);
  if (!parsed.has_value()) {
    send_error(response, httplib::StatusCode::BadRequest_400, "invalid json");
    return false;
  }

  if (body != nullptr) {
    *body = std::move(*parsed);
  }
  return true;
}

}  // namespace oj::util

#pragma once

#include "util/json.h"

#include <httplib.h>

#include <string>

namespace oj::util {

json::Value json_envelope(bool success, std::string message,
                          json::Value data = nullptr);

void send_json(httplib::Response& response, int status, const json::Value& body);

void send_success(httplib::Response& response, json::Value data = nullptr,
                  std::string message = "ok", int status = httplib::StatusCode::OK_200);

void send_error(httplib::Response& response, int status, std::string message,
                json::Value data = nullptr);

bool parse_json_body(const httplib::Request& request, json::Value* body,
                     httplib::Response& response);

}  // namespace oj::util

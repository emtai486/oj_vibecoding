#pragma once

#include "db/mysql_client.h"
#include "util/json_response.h"

#include <httplib.h>

#include <iostream>
#include <memory>
#include <string>

namespace oj::api {

inline void log_database_error(const httplib::Request& request,
                               const std::string& operation,
                               const std::string& error) {
  std::cerr << "database error: " << request.method << ' ' << request.path
            << " during " << operation << ": "
            << (error.empty() ? "unknown error" : error) << '\n';
}

inline void send_database_error(const httplib::Request& request,
                                httplib::Response& response,
                                const std::string& operation,
                                const std::string& error) {
  log_database_error(request, operation, error);
  oj::util::send_error(response,
                       httplib::StatusCode::InternalServerError_500,
                       "database error");
}

inline bool acquire_db(
    const std::shared_ptr<db::MySqlConnectionPool>& mysql_pool,
    const httplib::Request& request,
    httplib::Response& response,
    db::PooledMySqlClient* client) {
  std::string error;
  if (mysql_pool == nullptr || !mysql_pool->acquire(client, &error)) {
    send_database_error(request, response, "connect", error);
    return false;
  }
  return true;
}

}  // namespace oj::api

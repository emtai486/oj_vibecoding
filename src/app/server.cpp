#include "app/server.h"

#include "api/admin_api.h"
#include "api/problem_api.h"
#include "api/submit_api.h"
#include "api/user_api.h"
#include "auth/session.h"
#include "util/json_response.h"

#include <exception>
#include <iostream>
#include <memory>
#include <stdexcept>
#include <utility>

namespace oj::app {
namespace {

void register_static_files(httplib::Server& server,
                           const std::string& public_dir) {
  if (!server.set_mount_point("/", public_dir)) {
    throw std::runtime_error("failed to mount static directory: " + public_dir);
  }

  server.set_file_extension_and_mimetype_mapping("html",
                                                 "text/html; charset=utf-8");
  server.set_file_extension_and_mimetype_mapping("css",
                                                 "text/css; charset=utf-8");
  server.set_file_extension_and_mimetype_mapping("js",
                                                 "application/javascript; charset=utf-8");
}

void register_error_handlers(httplib::Server& server) {
  server.set_exception_handler(
      [](const httplib::Request&, httplib::Response& response,
         std::exception_ptr error) {
        try {
          if (error != nullptr) {
            std::rethrow_exception(error);
          }
        } catch (const std::exception& ex) {
          std::cerr << "request failed: " << ex.what() << '\n';
        }

        oj::util::send_error(response,
                             httplib::StatusCode::InternalServerError_500,
                             "internal server error");
      });

  server.set_error_handler([](const httplib::Request& request,
                              httplib::Response& response) {
    if (!response.body.empty()) {
      return;
    }

    if (request.path.rfind("/api/", 0) == 0) {
      const int status = response.status == -1
                             ? httplib::StatusCode::NotFound_404
                             : response.status;
      const std::string message =
          status == httplib::StatusCode::NotFound_404 ? "not found" : "failed";
      oj::util::send_error(response, status, message);
      return;
    }

    if (response.status == -1) {
      response.status = httplib::StatusCode::NotFound_404;
    }
    response.set_content("not found\n", "text/plain; charset=utf-8");
  });
}

void register_base_routes(httplib::Server& server) {
  server.Get("/health", [](const httplib::Request&, httplib::Response& response) {
    oj::util::send_success(response, oj::util::json::Value::Object{
                                         {"status", "ok"},
                                     });
  });

  server.Post("/api/_echo", [](const httplib::Request& request,
                               httplib::Response& response) {
    oj::util::json::Value body;
    if (!oj::util::parse_json_body(request, &body, response)) {
      return;
    }

    oj::util::send_success(response, std::move(body));
  });
}

}  // namespace

std::unique_ptr<httplib::Server> create_server(const config::AppConfig& config,
                                               std::string public_dir) {
  auto server = std::make_unique<httplib::Server>();
  auto sessions = std::make_shared<auth::SessionStore>();
  register_error_handlers(*server);
  register_static_files(*server, public_dir);
  register_base_routes(*server);
  api::register_problem_routes(*server, config.mysql);
  api::register_user_routes(*server, config.mysql, sessions);
  api::register_submit_routes(*server, config.mysql, sessions);
  api::register_admin_routes(*server, config.mysql, sessions);
  return server;
}

bool run_server(const config::AppConfig& config, std::string public_dir) {
  auto server = create_server(config, std::move(public_dir));
  std::cout << "oj server listening on " << config.server.host << ':'
            << config.server.port << '\n';
  return server->listen(config.server.host, config.server.port);
}

}  // namespace oj::app

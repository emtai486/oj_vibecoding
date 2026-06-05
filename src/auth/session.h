#pragma once

#include <cstdint>
#include <mutex>
#include <optional>
#include <string>
#include <unordered_map>

namespace httplib {
struct Request;
struct Response;
}  // namespace httplib

namespace oj::auth {

struct SessionUser {
  std::uint64_t id = 0;
  std::string username;
};

struct SessionAdmin {
  std::uint64_t id = 0;
  std::string username;
};

class SessionStore {
 public:
  std::string create_user_session(std::uint64_t user_id,
                                  const std::string& username);
  std::optional<SessionUser> find_user_session(const std::string& session_id);
  void destroy_user_session(const std::string& session_id);

  std::string create_admin_session(std::uint64_t admin_id,
                                   const std::string& username);
  std::optional<SessionAdmin> find_admin_session(const std::string& session_id);
  void destroy_admin_session(const std::string& session_id);

 private:
  std::mutex mutex_;
  std::unordered_map<std::string, SessionUser> user_sessions_;
  std::unordered_map<std::string, SessionAdmin> admin_sessions_;
};

std::optional<std::string> cookie_value(const httplib::Request& request,
                                        const std::string& name);

void set_user_session_cookie(httplib::Response& response,
                             const std::string& session_id);

void clear_user_session_cookie(httplib::Response& response);

void set_admin_session_cookie(httplib::Response& response,
                              const std::string& session_id);

void clear_admin_session_cookie(httplib::Response& response);

}  // namespace oj::auth

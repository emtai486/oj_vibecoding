#include "auth/session.h"

#include <httplib.h>

#include <chrono>
#include <random>
#include <sstream>
#include <utility>

namespace oj::auth {
namespace {

constexpr const char* kUserSessionCookie = "oj_user_session";
constexpr const char* kAdminSessionCookie = "oj_admin_session";

std::string trim(std::string value) {
  while (!value.empty() &&
         (value.front() == ' ' || value.front() == '\t' ||
          value.front() == '\r' || value.front() == '\n')) {
    value.erase(value.begin());
  }
  while (!value.empty() &&
         (value.back() == ' ' || value.back() == '\t' ||
          value.back() == '\r' || value.back() == '\n')) {
    value.pop_back();
  }
  return value;
}

std::string random_token() {
  std::random_device random;
  const auto now = std::chrono::steady_clock::now().time_since_epoch().count();
  std::ostringstream output;
  output << std::hex << random() << random() << random() << random() << now;
  return output.str();
}

}  // namespace

std::string SessionStore::create_user_session(std::uint64_t user_id,
                                              const std::string& username) {
  std::string session_id = random_token();
  std::lock_guard<std::mutex> lock(mutex_);
  while (user_sessions_.find(session_id) != user_sessions_.end() ||
         admin_sessions_.find(session_id) != admin_sessions_.end()) {
    session_id = random_token();
  }
  user_sessions_[session_id] = SessionUser{user_id, username};
  return session_id;
}

std::optional<SessionUser> SessionStore::find_user_session(
    const std::string& session_id) {
  std::lock_guard<std::mutex> lock(mutex_);
  const auto it = user_sessions_.find(session_id);
  if (it == user_sessions_.end()) {
    return std::nullopt;
  }
  return it->second;
}

void SessionStore::destroy_user_session(const std::string& session_id) {
  std::lock_guard<std::mutex> lock(mutex_);
  user_sessions_.erase(session_id);
}

std::string SessionStore::create_admin_session(std::uint64_t admin_id,
                                               const std::string& username) {
  std::string session_id = random_token();
  std::lock_guard<std::mutex> lock(mutex_);
  while (user_sessions_.find(session_id) != user_sessions_.end() ||
         admin_sessions_.find(session_id) != admin_sessions_.end()) {
    session_id = random_token();
  }
  admin_sessions_[session_id] = SessionAdmin{admin_id, username};
  return session_id;
}

std::optional<SessionAdmin> SessionStore::find_admin_session(
    const std::string& session_id) {
  std::lock_guard<std::mutex> lock(mutex_);
  const auto it = admin_sessions_.find(session_id);
  if (it == admin_sessions_.end()) {
    return std::nullopt;
  }
  return it->second;
}

void SessionStore::destroy_admin_session(const std::string& session_id) {
  std::lock_guard<std::mutex> lock(mutex_);
  admin_sessions_.erase(session_id);
}

std::optional<std::string> cookie_value(const httplib::Request& request,
                                        const std::string& name) {
  const std::string cookie = request.get_header_value("Cookie");
  std::size_t start = 0;
  while (start < cookie.size()) {
    const auto end = cookie.find(';', start);
    const std::string part = trim(cookie.substr(
        start, end == std::string::npos ? std::string::npos : end - start));
    const auto separator = part.find('=');
    if (separator != std::string::npos &&
        part.substr(0, separator) == name) {
      return part.substr(separator + 1);
    }
    if (end == std::string::npos) {
      break;
    }
    start = end + 1;
  }
  return std::nullopt;
}

void set_user_session_cookie(httplib::Response& response,
                             const std::string& session_id) {
  response.set_header("Set-Cookie",
                      std::string(kUserSessionCookie) + "=" + session_id +
                          "; Path=/; HttpOnly; SameSite=Lax");
}

void clear_user_session_cookie(httplib::Response& response) {
  response.set_header("Set-Cookie",
                      std::string(kUserSessionCookie) +
                          "=; Path=/; Max-Age=0; HttpOnly; SameSite=Lax");
}

void set_admin_session_cookie(httplib::Response& response,
                              const std::string& session_id) {
  response.set_header("Set-Cookie",
                      std::string(kAdminSessionCookie) + "=" + session_id +
                          "; Path=/; HttpOnly; SameSite=Lax");
}

void clear_admin_session_cookie(httplib::Response& response) {
  response.set_header("Set-Cookie",
                      std::string(kAdminSessionCookie) +
                          "=; Path=/; Max-Age=0; HttpOnly; SameSite=Lax");
}

}  // namespace oj::auth

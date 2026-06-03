#pragma once

#include <cstdint>
#include <string>

namespace oj::config {

struct ServerConfig {
  std::string host = "0.0.0.0";
  std::uint16_t port = 8080;
};

struct MySqlConfig {
  std::string host = "127.0.0.1";
  std::uint16_t port = 3306;
  std::string user = "oj_user";
  std::string password = "change_me";
  std::string database = "oj";
  std::string charset = "utf8mb4";
  std::uint32_t connect_timeout_seconds = 5;
};

struct AppConfig {
  ServerConfig server;
  MySqlConfig mysql;
};

AppConfig load_config(const std::string& path);

}  // namespace oj::config

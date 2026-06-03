#include "config/config.h"

#include <algorithm>
#include <cctype>
#include <fstream>
#include <stdexcept>
#include <string>
#include <string_view>
#include <unordered_map>

namespace oj::config {
namespace {

std::string trim(std::string_view value) {
  while (!value.empty() &&
         std::isspace(static_cast<unsigned char>(value.front())) != 0) {
    value.remove_prefix(1);
  }

  while (!value.empty() &&
         std::isspace(static_cast<unsigned char>(value.back())) != 0) {
    value.remove_suffix(1);
  }

  return std::string(value);
}

std::uint32_t parse_uint(const std::string& key, const std::string& value,
                         std::uint32_t min_value,
                         std::uint32_t max_value) {
  try {
    std::size_t parsed = 0;
    const unsigned long number = std::stoul(value, &parsed, 10);
    if (parsed != value.size() || number < min_value || number > max_value) {
      throw std::out_of_range("out of range");
    }
    return static_cast<std::uint32_t>(number);
  } catch (const std::exception&) {
    throw std::runtime_error("invalid numeric config value for " + key + ": " +
                             value);
  }
}

void assign_string(const std::unordered_map<std::string, std::string>& values,
                   const std::string& key, std::string& target) {
  const auto it = values.find(key);
  if (it != values.end()) {
    target = it->second;
  }
}

void assign_port(const std::unordered_map<std::string, std::string>& values,
                 const std::string& key, std::uint16_t& target) {
  const auto it = values.find(key);
  if (it != values.end()) {
    target = static_cast<std::uint16_t>(parse_uint(key, it->second, 1, 65535));
  }
}

void assign_uint(const std::unordered_map<std::string, std::string>& values,
                 const std::string& key, std::uint32_t& target,
                 std::uint32_t min_value, std::uint32_t max_value) {
  const auto it = values.find(key);
  if (it != values.end()) {
    target = parse_uint(key, it->second, min_value, max_value);
  }
}

}  // namespace

AppConfig load_config(const std::string& path) {
  std::ifstream input(path);
  if (!input) {
    throw std::runtime_error("failed to open config file: " + path);
  }

  std::unordered_map<std::string, std::string> values;
  std::string line;
  std::size_t line_number = 0;
  while (std::getline(input, line)) {
    ++line_number;

    const std::string trimmed = trim(line);
    if (trimmed.empty() || trimmed.front() == '#') {
      continue;
    }

    const auto separator = trimmed.find('=');
    if (separator == std::string::npos) {
      throw std::runtime_error("invalid config line " +
                               std::to_string(line_number) + " in " + path);
    }

    const std::string key = trim(std::string_view(trimmed).substr(0, separator));
    const std::string value =
        trim(std::string_view(trimmed).substr(separator + 1));
    if (key.empty()) {
      throw std::runtime_error("empty config key on line " +
                               std::to_string(line_number) + " in " + path);
    }

    values[key] = value;
  }

  AppConfig config;
  assign_string(values, "server.host", config.server.host);
  assign_port(values, "server.port", config.server.port);

  assign_string(values, "mysql.host", config.mysql.host);
  assign_port(values, "mysql.port", config.mysql.port);
  assign_string(values, "mysql.user", config.mysql.user);
  assign_string(values, "mysql.password", config.mysql.password);
  assign_string(values, "mysql.database", config.mysql.database);
  assign_string(values, "mysql.charset", config.mysql.charset);
  assign_uint(values, "mysql.connect_timeout_seconds",
              config.mysql.connect_timeout_seconds, 1, 60);

  return config;
}

}  // namespace oj::config

#include "app/server.h"
#include "config/config.h"
#include "db/mysql_client.h"

#include <exception>
#include <iostream>
#include <string>

namespace {

constexpr const char* kDefaultConfigPath = "config/app.example.conf";

void print_usage(const char* program) {
  std::cout << "Usage:\n"
            << "  " << program << " [config_path]\n"
            << "  " << program << " --check-db [config_path]\n";
}

}  // namespace

int main(int argc, char* argv[]) {
  bool check_db = false;
  std::string config_path = kDefaultConfigPath;

  if (argc >= 2) {
    const std::string first_arg = argv[1];
    if (first_arg == "--help" || first_arg == "-h") {
      print_usage(argv[0]);
      return 0;
    }
    if (first_arg == "--check-db") {
      check_db = true;
      if (argc >= 3) {
        config_path = argv[2];
      }
    } else {
      config_path = first_arg;
    }
  }

  try {
    const auto config = oj::config::load_config(config_path);

    if (check_db) {
      oj::db::MySqlClient client(config.mysql);
      std::string error;
      if (!client.connect(&error) || !client.ping(&error)) {
        std::cerr << "mysql connection failed: " << error << '\n';
        return 2;
      }

      std::cout << "mysql connection ok: " << config.mysql.host << ':'
                << config.mysql.port << '/' << config.mysql.database << '\n';
      return 0;
    }

    std::cout << "config loaded from " << config_path << '\n';
    if (!oj::app::run_server(config)) {
      std::cerr << "failed to start server on " << config.server.host << ':'
                << config.server.port << '\n';
      return 2;
    }
    return 0;
  } catch (const std::exception& error) {
    std::cerr << error.what() << '\n';
    return 1;
  }
}

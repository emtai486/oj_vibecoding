#pragma once

#include "config/config.h"

#include <httplib.h>

#include <memory>
#include <string>

namespace oj::app {

std::unique_ptr<httplib::Server> create_server(
    const config::AppConfig& config, std::string public_dir = "public");

bool run_server(const config::AppConfig& config,
                std::string public_dir = "public");

}  // namespace oj::app

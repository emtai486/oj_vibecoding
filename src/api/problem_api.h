#pragma once

#include "config/config.h"

#include <httplib.h>

namespace oj::api {

void register_problem_routes(httplib::Server& server,
                             config::MySqlConfig mysql_config);

}  // namespace oj::api

#pragma once

#include "config/config.h"
#include "db/mysql_client.h"

#include <httplib.h>

#include <memory>

namespace oj::api {

void register_problem_routes(httplib::Server& server,
                             std::shared_ptr<db::MySqlConnectionPool> mysql_pool);

}  // namespace oj::api

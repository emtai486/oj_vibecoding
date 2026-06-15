#pragma once

#include "auth/session.h"
#include "config/config.h"
#include "db/mysql_client.h"

#include <httplib.h>

#include <memory>

namespace oj::api {

void register_admin_routes(httplib::Server& server,
                           std::shared_ptr<db::MySqlConnectionPool> mysql_pool,
                           std::shared_ptr<auth::SessionStore> sessions);

}  // namespace oj::api

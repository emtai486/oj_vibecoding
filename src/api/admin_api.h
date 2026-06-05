#pragma once

#include "auth/session.h"
#include "config/config.h"

#include <httplib.h>

#include <memory>

namespace oj::api {

void register_admin_routes(httplib::Server& server,
                           config::MySqlConfig mysql_config,
                           std::shared_ptr<auth::SessionStore> sessions);

}  // namespace oj::api

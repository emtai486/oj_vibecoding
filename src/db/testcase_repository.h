#pragma once

#include "db/mysql_client.h"
#include "model/testcase.h"

#include <cstdint>
#include <string>
#include <vector>

namespace oj::db {

class TestcaseRepository {
 public:
  explicit TestcaseRepository(MySqlClient& client);

  bool list_for_problem(std::uint64_t problem_id, bool is_sample,
                        std::vector<model::Testcase>* testcases,
                        std::string* error);

 private:
  MySqlClient& client_;
};

}  // namespace oj::db

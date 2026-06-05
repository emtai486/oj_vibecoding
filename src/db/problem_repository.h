#pragma once

#include "db/mysql_client.h"
#include "model/problem.h"

#include <optional>
#include <string>
#include <vector>

namespace oj::db {

class ProblemRepository {
 public:
  explicit ProblemRepository(MySqlClient& client);

  bool list(std::vector<model::ProblemSummary>* problems, std::string* error);
  bool find_by_id(std::uint64_t id, std::optional<model::Problem>* problem,
                  std::string* error);

 private:
  MySqlClient& client_;
};

}  // namespace oj::db

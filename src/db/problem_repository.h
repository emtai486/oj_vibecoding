#pragma once

#include "db/mysql_client.h"
#include "model/problem.h"
#include "model/testcase.h"

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
  bool create_with_testcases(const model::Problem& problem,
                             const std::vector<model::Testcase>& testcases,
                             std::uint64_t* id, std::string* error);
  bool update_with_testcases(std::uint64_t id, const model::Problem& problem,
                             const std::vector<model::Testcase>& testcases,
                             bool* updated, std::string* error);
  bool delete_by_id(std::uint64_t id, bool* deleted, std::string* error);

 private:
  MySqlClient& client_;
};

}  // namespace oj::db

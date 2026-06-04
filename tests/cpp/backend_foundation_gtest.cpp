#include "app/server.h"
#include "config/config.h"
#include "db/mysql_client.h"
#include "util/json.h"
#include "util/json_response.h"

#include <gtest/gtest.h>
#include <httplib.h>

#include <cstdint>
#include <filesystem>
#include <string>

namespace {

std::filesystem::path project_root = std::filesystem::current_path();

TEST(JsonTest, ParsesRequestLikeBody) {
  std::string error;
  const auto parsed = oj::util::json::parse(
      R"({"username":"user1","problem_id":1,"active":true,"tags":["cpp"]})",
      &error);

  ASSERT_TRUE(parsed.has_value()) << error;
  ASSERT_TRUE(parsed->is_object());

  const auto& object = parsed->as_object();
  EXPECT_EQ(object.at("username").as_string(), "user1");
  EXPECT_EQ(object.at("problem_id").as_int(), std::int64_t{1});
  EXPECT_TRUE(object.at("active").as_bool());
  ASSERT_TRUE(object.at("tags").is_array());
  ASSERT_EQ(object.at("tags").as_array().size(), std::size_t{1});
  EXPECT_EQ(object.at("tags").as_array().front().as_string(), "cpp");
}

TEST(JsonTest, RejectsInvalidJson) {
  std::string error;
  const auto parsed = oj::util::json::parse(R"({"username":)", &error);

  EXPECT_FALSE(parsed.has_value());
  EXPECT_FALSE(error.empty());
}

TEST(JsonTest, StringifiesEscapedResponseData) {
  const auto encoded =
      oj::util::json::stringify(oj::util::json::Value::Object{
          {"message", "line\nquoted"},
          {"data", oj::util::json::Value::Array{
                       oj::util::json::Value(std::int64_t{1}),
                       oj::util::json::Value(std::int64_t{2}),
                   }},
      });

  EXPECT_NE(encoded.find(R"("message":"line\nquoted")"), std::string::npos);
  EXPECT_NE(encoded.find(R"("data":[1,2])"), std::string::npos);
}

TEST(JsonResponseTest, SendsSuccessEnvelope) {
  httplib::Response response;

  oj::util::send_success(response,
                         oj::util::json::Value::Object{{"status", "ok"}});

  EXPECT_EQ(response.status, static_cast<int>(httplib::StatusCode::OK_200));
  EXPECT_NE(response.get_header_value("Content-Type").find("application/json"),
            std::string::npos);
  EXPECT_NE(response.body.find(R"("success":true)"), std::string::npos);
  EXPECT_NE(response.body.find(R"("message":"ok")"), std::string::npos);
  EXPECT_NE(response.body.find(R"("status":"ok")"), std::string::npos);
}

TEST(JsonResponseTest, SendsErrorEnvelope) {
  httplib::Response response;

  oj::util::send_error(response, httplib::StatusCode::Unauthorized_401,
                       "unauthorized");

  EXPECT_EQ(response.status,
            static_cast<int>(httplib::StatusCode::Unauthorized_401));
  EXPECT_NE(response.body.find(R"("success":false)"), std::string::npos);
  EXPECT_NE(response.body.find(R"("message":"unauthorized")"),
            std::string::npos);
  EXPECT_NE(response.body.find(R"("data":null)"), std::string::npos);
}

TEST(JsonResponseTest, ParsesBodyOrReturnsBadRequest) {
  httplib::Request valid_request;
  valid_request.body = R"({"code":"int main(){return 0;}"})";
  httplib::Response valid_response;
  oj::util::json::Value body;

  EXPECT_TRUE(
      oj::util::parse_json_body(valid_request, &body, valid_response));
  EXPECT_EQ(body.as_object().at("code").as_string(), "int main(){return 0;}");

  httplib::Request invalid_request;
  invalid_request.body = R"({"code":)";
  httplib::Response invalid_response;

  EXPECT_FALSE(
      oj::util::parse_json_body(invalid_request, nullptr, invalid_response));
  EXPECT_EQ(invalid_response.status,
            static_cast<int>(httplib::StatusCode::BadRequest_400));
  EXPECT_NE(invalid_response.body.find(R"("message":"invalid json")"),
            std::string::npos);
}

TEST(MySqlClientTest, DisconnectedOperationsFailClearly) {
  oj::config::MySqlConfig config;
  oj::db::MySqlClient client(config);
  std::string error;

  EXPECT_FALSE(client.is_connected());
  EXPECT_FALSE(client.ping(&error));
  EXPECT_EQ(error, "mysql connection is not open");

  EXPECT_FALSE(client.execute("SELECT 1", &error));
  EXPECT_EQ(error, "mysql connection is not open");

  oj::db::QueryResult result;
  EXPECT_FALSE(client.query("SELECT 1", &result, &error));
  EXPECT_EQ(error, "mysql connection is not open");
}

TEST(ServerTest, CreatesValidServerWithStaticRoot) {
  const auto config =
      oj::config::load_config((project_root / "config/app.example.conf").string());

  const auto server =
      oj::app::create_server(config, (project_root / "public").string());

  ASSERT_NE(server, nullptr);
  EXPECT_TRUE(server->is_valid());
}

}  // namespace

int main(int argc, char** argv) {
  testing::InitGoogleTest(&argc, argv);
  if (argc >= 2) {
    project_root = std::filesystem::absolute(argv[1]);
  }

  return RUN_ALL_TESTS();
}

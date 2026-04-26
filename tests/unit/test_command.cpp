#include "command/command.h"

#include <gtest/gtest.h>

#include <stdexcept>
#include <string>

#include "store/kv_store.h"

namespace {

using kv::command::Json;
using kv::command::execute_command;
using kv::store::KVStore;

TEST(CommandExecutionTest, PutStoresRawStringValue) {
  KVStore store;
  const Json response = execute_command(
      Json{{"action", "put"},
           {"params", Json{{"key", "alpha"}, {"value", "hello world"}}}},
      store);

  EXPECT_TRUE(response.at("ok").get<bool>());
  EXPECT_EQ("put", response.at("action").get<std::string>());
  EXPECT_EQ("alpha", response.at("key").get<std::string>());
  ASSERT_TRUE(store.Get("alpha").has_value());
  EXPECT_EQ("hello world", store.Get("alpha").value());
}

TEST(CommandExecutionTest, PutSerializesStructuredJsonValues) {
  KVStore store;
  const Json response = execute_command(
      Json{{"action", "put"},
           {"params",
            Json{{"key", "config"},
                 {"value", Json{{"enabled", true}, {"retries", 3}}}}}},
      store);

  EXPECT_TRUE(response.at("ok").get<bool>());
  ASSERT_TRUE(store.Get("config").has_value());
  EXPECT_EQ(Json({{"enabled", true}, {"retries", 3}}).dump(),
            store.Get("config").value());
}

TEST(CommandExecutionTest, GetReturnsStoredValueWhenPresent) {
  KVStore store;
  store.Set("alpha", "1");

  const Json response = execute_command(
      Json{{"action", "get"}, {"params", Json{{"key", "alpha"}}}}, store);

  EXPECT_TRUE(response.at("ok").get<bool>());
  EXPECT_EQ("1", response.at("value").get<std::string>());
}

TEST(CommandExecutionTest, GetReturnsNotFoundResponseWhenKeyIsMissing) {
  KVStore store;

  const Json response = execute_command(
      Json{{"action", "get"}, {"params", Json{{"key", "missing"}}}}, store);

  EXPECT_FALSE(response.at("ok").get<bool>());
  EXPECT_EQ("not found", response.at("error").get<std::string>());
}

TEST(CommandExecutionTest, DeleteRemovesStoredKey) {
  KVStore store;
  store.Set("alpha", "1");

  const Json response = execute_command(
      Json{{"action", "delete"}, {"params", Json{{"key", "alpha"}}}}, store);

  EXPECT_TRUE(response.at("ok").get<bool>());
  EXPECT_FALSE(store.Contains("alpha"));
}

TEST(CommandExecutionTest, RoutesSessionAwareActionsThroughAgentSessionManager) {
  KVStore store;

  const Json begin_response = execute_command(
      Json{{"action", "begin_session"},
           {"params", Json{{"initial_state", Json{{"goal", "write tests"}}}}}},
      store);
  EXPECT_TRUE(begin_response.at("ok").get<bool>());
  const std::string session_id =
      begin_response.at("session_id").get<std::string>();

  const Json log_response = execute_command(
      Json{{"action", "log_step"},
           {"params",
            Json{{"session_id", session_id},
                 {"action", "write_file"},
                 {"input", Json{{"path", "tests/session_test.cpp"}}},
                 {"output", Json{{"result", "created test file"}}},
                 {"state_diff",
                  Json{{"status", "testing"},
                       {"last_file", "tests/session_test.cpp"}}}}}},
      store);
  EXPECT_TRUE(log_response.at("ok").get<bool>());
  EXPECT_EQ(1u, log_response.at("event").at("seq").get<std::size_t>());

  const Json context_response = execute_command(
      Json{{"action", "get_context"},
           {"params", Json{{"session_id", session_id}, {"limit", 1}}}},
      store);

  EXPECT_TRUE(context_response.at("ok").get<bool>());
  ASSERT_TRUE(context_response.at("context").contains("state"));
  ASSERT_TRUE(context_response.at("context").contains("metadata"));
  ASSERT_TRUE(context_response.at("context").contains("recent_steps"));
  EXPECT_EQ("testing",
            context_response.at("context")
                .at("state")
                .at("status")
                .get<std::string>());
  ASSERT_EQ(1u, context_response.at("context").at("recent_steps").size());
  EXPECT_EQ("write_file",
            context_response.at("context")
                .at("recent_steps")
                .at(0)
                .at("action")
                .get<std::string>());
}

TEST(CommandExecutionTest, UnknownSessionIdReturnsStructuredNotFoundResponse) {
  KVStore store;
  const Json response = execute_command(
      Json{{"action", "get_context"},
           {"params", Json{{"session_id", "missing"}, {"limit", 1}}}},
      store);

  EXPECT_FALSE(response.at("ok").get<bool>());
  EXPECT_EQ("session not found", response.at("error").get<std::string>());
}

TEST(CommandExecutionTest, MissingRequiredParamThrowsClearError) {
  KVStore store;

  EXPECT_THROW(
      {
        try {
          execute_command(
              Json{{"action", "get_state"}, {"params", Json::object()}},
              store);
        } catch (const std::invalid_argument& error) {
          EXPECT_STREQ("request.params.session_id is required", error.what());
          throw;
        }
      },
      std::invalid_argument);
}

TEST(CommandExecutionTest, UnsupportedActionThrowsClearError) {
  KVStore store;

  EXPECT_THROW(
      {
        try {
          execute_command(
              Json{{"action", "unknown"}, {"params", Json::object()}}, store);
        } catch (const std::invalid_argument& error) {
          EXPECT_STREQ("unsupported action: unknown", error.what());
          throw;
        }
      },
      std::invalid_argument);
}

}  // namespace

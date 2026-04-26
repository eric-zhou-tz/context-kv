#include "agent/session.h"

#include <gtest/gtest.h>

#include <stdexcept>
#include <string>

#include "store/kv_store.h"

namespace {

using kv::agent::AgentSessionManager;
using kv::agent::Json;
using kv::store::KVStore;

TEST(AgentSessionManagerTest, BeginSessionCreatesOneStoredSessionDocument) {
  KVStore store;
  AgentSessionManager manager(store);

  const Json response =
      manager.BeginSession(Json{{"goal", "write tests"}, {"status", "started"}});

  ASSERT_TRUE(response.at("ok").get<bool>());
  const std::string session_id = response.at("session_id").get<std::string>();
  const std::string key = "sessions/" + session_id;

  ASSERT_TRUE(store.Get(key).has_value());
  const Json stored = Json::parse(store.Get(key).value());
  EXPECT_EQ("write tests", stored.at("state").at("goal").get<std::string>());
  EXPECT_TRUE(stored.at("events").empty());
  EXPECT_EQ(session_id,
            stored.at("metadata").at("session_id").get<std::string>());
}

TEST(AgentSessionManagerTest, GetStateReturnsInitialState) {
  KVStore store;
  AgentSessionManager manager(store);

  const Json begin =
      manager.BeginSession(Json{{"goal", "write tests"}, {"status", "started"}});
  const std::string session_id = begin.at("session_id").get<std::string>();

  const Json response = manager.GetState(session_id);

  EXPECT_EQ(Json({{"goal", "write tests"}, {"status", "started"}}),
            response.at("state"));
}

TEST(AgentSessionManagerTest, UpdateStateShallowMergesStateDiff) {
  KVStore store;
  AgentSessionManager manager(store);

  const Json begin =
      manager.BeginSession(Json{{"goal", "write tests"}, {"status", "started"}});
  const std::string session_id = begin.at("session_id").get<std::string>();

  const Json response = manager.UpdateState(
      session_id, Json{{"status", "testing"}, {"last_file", "session_test.cpp"}});

  EXPECT_EQ(
      Json({{"goal", "write tests"},
            {"status", "testing"},
            {"last_file", "session_test.cpp"}}),
      response.at("state"));
}

TEST(AgentSessionManagerTest, LogStepAppendsEventAndUpdatesState) {
  KVStore store;
  AgentSessionManager manager(store);

  const Json begin = manager.BeginSession(Json{{"status", "started"}});
  const std::string session_id = begin.at("session_id").get<std::string>();

  const Json response = manager.LogStep(
      session_id, "write_file", Json{{"path", "tests/session_test.cpp"}},
      Json{{"result", "created test file"}},
      Json{{"status", "testing"}, {"last_file", "tests/session_test.cpp"}});

  EXPECT_EQ("write_file", response.at("event").at("action").get<std::string>());
  EXPECT_EQ(1u, response.at("event").at("seq").get<std::size_t>());
  EXPECT_EQ("testing", response.at("state").at("status").get<std::string>());
}

TEST(AgentSessionManagerTest,
     GetRecentStepsReturnsNewestEventsInChronologicalOrder) {
  KVStore store;
  AgentSessionManager manager(store);

  const Json begin = manager.BeginSession();
  const std::string session_id = begin.at("session_id").get<std::string>();

  manager.LogStep(session_id, "a", Json::object(), Json::object(),
                  Json{{"last_action", "a"}});
  manager.LogStep(session_id, "b", Json::object(), Json::object(),
                  Json{{"last_action", "b"}});
  manager.LogStep(session_id, "c", Json::object(), Json::object(),
                  Json{{"last_action", "c"}});

  const Json response = manager.GetRecentSteps(session_id, 2);

  ASSERT_EQ(2u, response.at("steps").size());
  EXPECT_EQ("b", response.at("steps").at(0).at("action").get<std::string>());
  EXPECT_EQ("c", response.at("steps").at(1).at("action").get<std::string>());
}

TEST(AgentSessionManagerTest, GetContextReturnsStateAndRecentEvents) {
  KVStore store;
  AgentSessionManager manager(store);

  const Json begin = manager.BeginSession(Json{{"goal", "write tests"}});
  const std::string session_id = begin.at("session_id").get<std::string>();
  manager.LogStep(session_id, "a", Json::object(), Json::object(),
                  Json{{"status", "drafting"}});
  manager.LogStep(session_id, "b", Json::object(), Json::object(),
                  Json{{"status", "testing"}});

  const Json response = manager.GetContext(session_id, 1);

  ASSERT_TRUE(response.at("context").contains("state"));
  ASSERT_TRUE(response.at("context").contains("metadata"));
  ASSERT_TRUE(response.at("context").contains("recent_steps"));
  EXPECT_EQ("testing",
            response.at("context").at("state").at("status").get<std::string>());
  ASSERT_EQ(1u, response.at("context").at("recent_steps").size());
  EXPECT_EQ("b",
            response.at("context")
                .at("recent_steps")
                .at(0)
                .at("action")
                .get<std::string>());
}

TEST(AgentSessionManagerTest, ReplayMatchesMaterializedState) {
  KVStore store;
  AgentSessionManager manager(store);

  const Json begin = manager.BeginSession();
  const std::string session_id = begin.at("session_id").get<std::string>();
  manager.LogStep(session_id, "search", Json::object(), Json::object(),
                  Json{{"status", "searching"}});
  manager.LogStep(session_id, "write", Json::object(), Json::object(),
                  Json{{"last_file", "tests/session_test.cpp"}});
  manager.LogStep(session_id, "finish", Json::object(), Json::object(),
                  Json{{"status", "done"}});

  const Json response = manager.Replay(session_id);

  EXPECT_EQ(3u, response.at("events_replayed").get<std::size_t>());
  EXPECT_EQ(response.at("replayed_state"), response.at("current_state"));
}

TEST(AgentSessionManagerTest, MissingSessionIdThrows) {
  KVStore store;
  AgentSessionManager manager(store);

  EXPECT_THROW(
      {
        try {
          manager.GetState("");
        } catch (const std::invalid_argument& error) {
          EXPECT_STREQ("session_id is required", error.what());
          throw;
        }
      },
      std::invalid_argument);
}

TEST(AgentSessionManagerTest, UnknownSessionIdReturnsStructuredError) {
  KVStore store;
  AgentSessionManager manager(store);

  const Json response = manager.GetState("missing");

  EXPECT_FALSE(response.at("ok").get<bool>());
  EXPECT_EQ("session not found", response.at("error").get<std::string>());
}

}  // namespace

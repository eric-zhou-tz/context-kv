#include "parser/command_parser.h"

#include <gtest/gtest.h>

#include <cctype>
#include <filesystem>
#include <sstream>
#include <stdexcept>
#include <string>
#include <sys/wait.h>
#include <unistd.h>
#include <vector>

namespace {

using Json = kv::parser::Json;

constexpr char kBinaryPath[] = "./bin/kv_store";

struct ProcessResult {
  int exit_code = -1;
  std::string output;
};

std::string TrimTrailingWhitespace(std::string text) {
  while (!text.empty() &&
         std::isspace(static_cast<unsigned char>(text.back())) != 0) {
    text.pop_back();
  }
  return text;
}

ProcessResult RunOneShotRequest(const std::string& db_path,
                                const std::string& raw_request) {
  int stdin_pipe[2];
  int stdout_pipe[2];
  if (pipe(stdin_pipe) != 0 || pipe(stdout_pipe) != 0) {
    throw std::runtime_error("failed to create process pipes");
  }

  const pid_t pid = fork();
  if (pid < 0) {
    throw std::runtime_error("failed to fork process");
  }

  if (pid == 0) {
    dup2(stdin_pipe[0], STDIN_FILENO);
    dup2(stdout_pipe[1], STDOUT_FILENO);
    close(stdin_pipe[0]);
    close(stdin_pipe[1]);
    close(stdout_pipe[0]);
    close(stdout_pipe[1]);
    execl(kBinaryPath, kBinaryPath, "--db", db_path.c_str(),
          static_cast<char*>(nullptr));
    _exit(127);
  }

  close(stdin_pipe[0]);
  close(stdout_pipe[1]);

  const std::string request_with_newline = raw_request + "\n";
  const ssize_t written =
      write(stdin_pipe[1], request_with_newline.data(),
            static_cast<size_t>(request_with_newline.size()));
  (void)written;
  close(stdin_pipe[1]);

  std::string output;
  char buffer[512];
  ssize_t read_count = 0;
  while ((read_count = read(stdout_pipe[0], buffer, sizeof(buffer))) > 0) {
    output.append(buffer, static_cast<size_t>(read_count));
  }
  close(stdout_pipe[0]);

  int status = 0;
  waitpid(pid, &status, 0);

  ProcessResult result;
  result.exit_code =
      WIFEXITED(status) ? WEXITSTATUS(status) : (128 + WTERMSIG(status));
  result.output = TrimTrailingWhitespace(output);
  return result;
}

Json ParseJsonOutput(const ProcessResult& result) {
  return Json::parse(result.output);
}

class AgentOverlayPersistenceTest : public ::testing::Test {
 protected:
  AgentOverlayPersistenceTest()
      : db_root_(std::filesystem::path("data") /
                 "test_agent_overlay_persistence") {}

  void SetUp() override {
    std::filesystem::remove_all(db_root_);
    std::filesystem::create_directories(db_root_);
  }

  void TearDown() override {
    std::filesystem::remove_all(db_root_);
  }

  Json Execute(const Json& request) const {
    const ProcessResult result =
        RunOneShotRequest(db_root_.string(), request.dump());
    EXPECT_EQ(0, result.exit_code) << result.output;
    return ParseJsonOutput(result);
  }

  std::filesystem::path db_root_;
};

TEST_F(AgentOverlayPersistenceTest,
       SessionContextPersistsAcrossRealProcessRestarts) {
  Json response = Execute(
      Json{{"action", "begin_session"}, {"params", Json::object()}});
  ASSERT_TRUE(response.at("ok").get<bool>());
  const std::string session_id = response.at("session_id").get<std::string>();

  response = Execute(
      Json{{"action", "log_step"},
           {"params",
            Json{{"session_id", session_id},
                 {"action", "draft_test"},
                 {"input", Json{{"path", "tests/session_test.cpp"}}},
                 {"output", Json{{"result", "drafted"}}},
                 {"state_diff",
                  Json{{"status", "drafting"},
                       {"last_file", "tests/session_test.cpp"}}}}}});
  EXPECT_TRUE(response.at("ok").get<bool>());

  response = Execute(
      Json{{"action", "log_step"},
           {"params",
            Json{{"session_id", session_id},
                 {"action", "run_tests"},
                 {"input", Json{{"target", "session tests"}}},
                 {"output", Json{{"result", "1 failure"}}},
                 {"state_diff", Json{{"status", "testing"}}}}}});
  EXPECT_TRUE(response.at("ok").get<bool>());

  response = Execute(
      Json{{"action", "log_step"},
           {"params",
            Json{{"session_id", session_id},
                 {"action", "fix_failure"},
                 {"input", Json{{"file", "tests/session_test.cpp"}}},
                 {"output", Json{{"result", "fixed"}}},
                 {"state_diff", Json{{"status", "done"}}}}}});
  EXPECT_TRUE(response.at("ok").get<bool>());

  const Json context_response = Execute(
      Json{{"action", "get_context"},
           {"params", Json{{"session_id", session_id}, {"limit", 5}}}});

  EXPECT_TRUE(context_response.at("ok").get<bool>());
  EXPECT_EQ("done",
            context_response.at("context")
                .at("state")
                .at("status")
                .get<std::string>());
  ASSERT_EQ(3u,
            context_response.at("context").at("recent_steps").size());

  const Json replay_response = Execute(
      Json{{"action", "replay"},
           {"params", Json{{"session_id", session_id}}}});
  EXPECT_EQ(replay_response.at("replayed_state"),
            replay_response.at("current_state"));

  EXPECT_TRUE(std::filesystem::exists(db_root_ / "kv_store.wal"));
}

TEST_F(AgentOverlayPersistenceTest, MissingSessionIdReturnsStructuredJsonError) {
  const ProcessResult result = RunOneShotRequest(
      db_root_.string(),
      Json{{"action", "get_context"}, {"params", Json::object()}}.dump());

  EXPECT_EQ(1, result.exit_code);
  const Json response = ParseJsonOutput(result);
  EXPECT_FALSE(response.at("ok").get<bool>());
  EXPECT_EQ("request.params.session_id is required",
            response.at("error").get<std::string>());
}

TEST_F(AgentOverlayPersistenceTest, UnknownSessionIdReturnsStructuredError) {
  const Json response = Execute(
      Json{{"action", "replay"},
           {"params", Json{{"session_id", "missing_session"}}}});

  EXPECT_FALSE(response.at("ok").get<bool>());
  EXPECT_EQ("session not found", response.at("error").get<std::string>());
}

}  // namespace

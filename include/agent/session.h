#ifndef KV_STORE_AGENT_SESSION_H_
#define KV_STORE_AGENT_SESSION_H_

#include <cstddef>
#include <string>

#include "parser/command_parser.h"
#include "store/kv_store.h"

namespace kv {
namespace agent {

using Json = parser::Json;

/**
 * @brief Manages Phase 2.5 agent sessions on top of the generic KV store.
 *
 * Each session is persisted as one JSON document under one KV key:
 * `sessions/<session_id>`. That keeps every session mutation mapped to one
 * `KVStore::Set()` call and therefore one WAL record.
 */
class AgentSessionManager {
 public:
  /**
   * @brief Constructs a session manager backed by the provided KV store.
   *
   * @param kv Generic KV store used for all session persistence.
   */
  explicit AgentSessionManager(store::KVStore& kv);

  /**
   * @brief Starts a new agent session and persists it as one JSON document.
   *
   * The whole session is stored under one KV key so each mutation maps to one
   * KV Set operation and one WAL record. This keeps Phase 2.5 crash recovery
   * simple and avoids partial session updates.
   *
   * @param initial_state Optional initial state object for the new session.
   * @return JSON response containing the new session identifier and state.
   */
  Json BeginSession(const Json& initial_state = Json::object());

  /**
   * @brief Loads the materialized state for an existing session.
   *
   * @param session_id Session identifier to load.
   * @return JSON response containing the current state object.
   */
  Json GetState(const std::string& session_id) const;

  /**
   * @brief Applies a shallow state diff to an existing session.
   *
   * @param session_id Session identifier to update.
   * @param state_diff Shallow object diff to merge into the session state.
   * @return JSON response containing the updated state.
   */
  Json UpdateState(const std::string& session_id, const Json& state_diff);

  /**
   * @brief Appends one event and materializes its state diff in one write.
   *
   * @param session_id Session identifier that owns the event stream.
   * @param action Action name recorded for the step.
   * @param input Input payload recorded for the step.
   * @param output Output payload recorded for the step.
   * @param state_diff Shallow object diff to merge into the session state.
   * @return JSON response containing the appended event and updated state.
   */
  Json LogStep(const std::string& session_id, const std::string& action,
               const Json& input, const Json& output, const Json& state_diff);

  /**
   * @brief Returns the most recent events for a session in chronological order.
   *
   * @param session_id Session identifier to inspect.
   * @param limit Maximum number of events to return.
   * @return JSON response containing the newest `limit` events.
   */
  Json GetRecentSteps(const std::string& session_id,
                      std::size_t limit = 5) const;

  /**
   * @brief Returns the main agent-facing context payload for a session.
   *
   * @param session_id Session identifier to load.
   * @param limit Maximum number of recent events to include.
   * @return JSON response containing metadata, state, and recent steps.
   */
  Json GetContext(const std::string& session_id,
                  std::size_t limit = 5) const;

  /**
   * @brief Reconstructs state by replaying stored event diffs in order.
   *
   * @param session_id Session identifier to replay.
   * @return JSON response containing replayed and materialized state.
   */
  Json Replay(const std::string& session_id) const;

 private:
  /** @brief Backing KV store used for all session persistence. */
  store::KVStore& kv_;
};

}  // namespace agent
}  // namespace kv

#endif  // KV_STORE_AGENT_SESSION_H_

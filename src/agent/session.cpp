#include "agent/session.h"

#include <chrono>
#include <cstdint>
#include <ctime>
#include <iomanip>
#include <optional>
#include <sstream>
#include <stdexcept>
#include <string>

namespace kv {
namespace agent {

namespace {

/**
 * @brief Validates a session identifier before it is used in a KV key.
 *
 * @param session_id Session identifier to validate.
 * @throws std::invalid_argument When the identifier is empty or contains `/`.
 */
void ValidateSessionId(const std::string& session_id) {
  if (session_id.empty()) {
    throw std::invalid_argument("session_id is required");
  }
  if (session_id.find('/') != std::string::npos) {
    throw std::invalid_argument("session_id must not contain '/'");
  }
}

/**
 * @brief Returns the single KV key used to store one full session document.
 *
 * @param session_id Validated session identifier.
 * @return Session document key.
 */
std::string SessionKey(const std::string& session_id) {
  return "sessions/" + session_id;
}

/**
 * @brief Returns the canonical empty session state object.
 *
 * @return Empty JSON object.
 */
Json EmptyState() {
  return Json::object();
}

/**
 * @brief Formats the current time as an ISO-8601 UTC timestamp.
 *
 * @return Timestamp string such as `2026-04-26T12:00:00Z`.
 */
std::string CurrentTimestamp() {
  const std::time_t now = std::time(nullptr);
  const std::tm utc_now = *std::gmtime(&now);

  std::ostringstream output;
  output << std::put_time(&utc_now, "%Y-%m-%dT%H:%M:%SZ");
  return output.str();
}

/**
 * @brief Generates a lightweight session identifier for a new session.
 *
 * @return Generated session identifier.
 */
std::string GenerateSessionId() {
  static std::uint64_t counter = 0;

  const auto now = std::chrono::system_clock::now().time_since_epoch();
  const auto ticks =
      std::chrono::duration_cast<std::chrono::nanoseconds>(now).count();

  std::ostringstream output;
  output << "session_" << ticks << "_" << ++counter;
  return output.str();
}

/**
 * @brief Validates that a value is a JSON object.
 *
 * @param value Value to validate.
 * @param field_name Human-readable field name for error messages.
 * @return The original value when it is an object.
 * @throws std::invalid_argument When the value is not an object.
 */
const Json& RequireObject(const Json& value, const std::string& field_name) {
  if (!value.is_object()) {
    throw std::invalid_argument(field_name + " must be a JSON object");
  }
  return value;
}

/**
 * @brief Applies a shallow merge of `state_diff` onto `state`.
 *
 * @param state Current state object.
 * @param state_diff Shallow state diff object.
 * @return Updated state object.
 * @throws std::invalid_argument When `state_diff` is not an object.
 */
Json ApplyStateDiff(Json state, const Json& state_diff) {
  RequireObject(state_diff, "state_diff");
  if (!state.is_object()) {
    state = EmptyState();
  }

  for (auto it = state_diff.begin(); it != state_diff.end(); ++it) {
    state[it.key()] = it.value();
  }
  return state;
}

/**
 * @brief Builds a standard success response prefix for session actions.
 *
 * @param action Executed action name.
 * @param session_id Target session identifier.
 * @return Success response prefix.
 */
Json MakeOkResponse(const std::string& action, const std::string& session_id) {
  return Json{
      {"ok", true},
      {"action", action},
      {"session_id", session_id},
  };
}

/**
 * @brief Builds a standard not-found response for missing sessions.
 *
 * @param action Executed action name.
 * @param session_id Session identifier that was not found.
 * @return Not-found response payload.
 */
Json MakeNotFoundResponse(const std::string& action,
                          const std::string& session_id) {
  return Json{
      {"ok", false},
      {"action", action},
      {"session_id", session_id},
      {"error", "session not found"},
  };
}

/**
 * @brief Parses one stored session document and validates its core shape.
 *
 * @param stored Serialized session document.
 * @param session_id Session identifier used in error messages.
 * @return Parsed and validated session document.
 * @throws std::invalid_argument When the stored document is malformed.
 */
Json ParseSessionDocument(const std::string& stored,
                          const std::string& session_id) {
  Json session;
  try {
    session = Json::parse(stored);
  } catch (const Json::parse_error&) {
    throw std::invalid_argument("stored session is not valid JSON: " +
                                session_id);
  }

  if (!session.is_object()) {
    throw std::invalid_argument("stored session must be a JSON object: " +
                                session_id);
  }
  if (!session.contains("metadata") || !session.at("metadata").is_object()) {
    throw std::invalid_argument("stored session metadata is invalid: " +
                                session_id);
  }
  if (!session.contains("state") || !session.at("state").is_object()) {
    throw std::invalid_argument("stored session state is invalid: " +
                                session_id);
  }
  if (!session.contains("events") || !session.at("events").is_array()) {
    throw std::invalid_argument("stored session events are invalid: " +
                                session_id);
  }

  return session;
}

/**
 * @brief Loads one session document from the KV store when present.
 *
 * @param kv Generic KV store used for persistence.
 * @param session_id Session identifier to load.
 * @return Parsed session document or `std::nullopt` when absent.
 */
std::optional<Json> LoadSession(const store::KVStore& kv,
                                const std::string& session_id) {
  const std::optional<std::string> stored = kv.Get(SessionKey(session_id));
  if (!stored.has_value()) {
    return std::nullopt;
  }
  return ParseSessionDocument(*stored, session_id);
}

/**
 * @brief Persists a full session document back under its single KV key.
 *
 * @param kv Generic KV store used for persistence.
 * @param session_id Session identifier being updated.
 * @param session Full session document to persist.
 */
void SaveSession(store::KVStore& kv, const std::string& session_id,
                 const Json& session) {
  kv.Set(SessionKey(session_id), session.dump());
}

/**
 * @brief Creates a new one-document session payload.
 *
 * @param session_id Generated session identifier.
 * @param initial_state Initial state object for the session.
 * @param timestamp Creation timestamp reused for metadata fields.
 * @return New session document.
 */
Json BuildSessionDocument(const std::string& session_id,
                          const Json& initial_state,
                          const std::string& timestamp) {
  return Json{
      {"metadata",
       Json{{"session_id", session_id},
            {"status", "active"},
            {"created_at", timestamp},
            {"updated_at", timestamp},
            {"last_seq", 0}}},
      {"state", initial_state},
      {"events", Json::array()},
  };
}

/**
 * @brief Returns the newest `limit` events while preserving chronological order.
 *
 * @param events Full event array from a session document.
 * @param limit Maximum number of events to include.
 * @return JSON array containing the requested suffix of events.
 */
Json SelectRecentEvents(const Json& events, std::size_t limit) {
  Json recent_events = Json::array();
  if (!events.is_array() || limit == 0 || events.empty()) {
    return recent_events;
  }

  const std::size_t start_index =
      (events.size() <= limit) ? 0 : (events.size() - limit);
  for (std::size_t index = start_index; index < events.size(); ++index) {
    recent_events.push_back(events.at(index));
  }
  return recent_events;
}

}  // namespace

AgentSessionManager::AgentSessionManager(store::KVStore& kv) : kv_(kv) {}

Json AgentSessionManager::BeginSession(const Json& initial_state) {
  RequireObject(initial_state, "initial_state");

  const std::string session_id = GenerateSessionId();
  const std::string timestamp = CurrentTimestamp();
  const Json session =
      BuildSessionDocument(session_id, initial_state, timestamp);

  // Persist the whole session as one document so one logical mutation becomes
  // one KV write and one WAL record.
  SaveSession(kv_, session_id, session);

  Json response = MakeOkResponse("begin_session", session_id);
  response["key"] = SessionKey(session_id);
  response["state"] = session.at("state");
  response["metadata"] = session.at("metadata");
  return response;
}

Json AgentSessionManager::GetState(const std::string& session_id) const {
  ValidateSessionId(session_id);

  // Load the single session document and read its materialized state.
  const std::optional<Json> session = LoadSession(kv_, session_id);
  if (!session.has_value()) {
    return MakeNotFoundResponse("get_state", session_id);
  }

  Json response = MakeOkResponse("get_state", session_id);
  response["state"] = session->at("state");
  return response;
}

Json AgentSessionManager::UpdateState(const std::string& session_id,
                                      const Json& state_diff) {
  ValidateSessionId(session_id);
  RequireObject(state_diff, "state_diff");

  // Load the existing session document before applying the shallow diff.
  const std::optional<Json> stored_session = LoadSession(kv_, session_id);
  if (!stored_session.has_value()) {
    return MakeNotFoundResponse("update_state", session_id);
  }

  Json session = *stored_session;
  session["state"] = ApplyStateDiff(session.at("state"), state_diff);
  session["metadata"]["updated_at"] = CurrentTimestamp();

  // Write the full document back in one Set so the state update is atomic at
  // the KV-operation and WAL-record level.
  SaveSession(kv_, session_id, session);

  Json response = MakeOkResponse("update_state", session_id);
  response["state"] = session.at("state");
  response["metadata"] = session.at("metadata");
  return response;
}

Json AgentSessionManager::LogStep(const std::string& session_id,
                                  const std::string& action,
                                  const Json& input, const Json& output,
                                  const Json& state_diff) {
  ValidateSessionId(session_id);
  if (action.empty()) {
    throw std::invalid_argument("action must be a non-empty string");
  }
  RequireObject(input, "input");
  RequireObject(output, "output");
  RequireObject(state_diff, "state_diff");

  // Load the current session document before appending a new event.
  const std::optional<Json> stored_session = LoadSession(kv_, session_id);
  if (!stored_session.has_value()) {
    return MakeNotFoundResponse("log_step", session_id);
  }

  Json session = *stored_session;
  const std::uint64_t next_seq =
      session.at("metadata").at("last_seq").get<std::uint64_t>() + 1;

  // Append one canonical event to the in-document event log.
  const Json event = {
      {"seq", next_seq},
      {"timestamp", CurrentTimestamp()},
      {"action", action},
      {"input", input},
      {"output", output},
      {"state_diff", state_diff},
  };
  session["events"].push_back(event);

  // Materialize the state diff so reads do not need to replay every event.
  session["state"] = ApplyStateDiff(session.at("state"), state_diff);
  session["metadata"]["last_seq"] = next_seq;
  session["metadata"]["updated_at"] = event.at("timestamp");

  // Persist the whole updated session as one document for crash safety.
  SaveSession(kv_, session_id, session);

  Json response = MakeOkResponse("log_step", session_id);
  response["event"] = event;
  response["state"] = session.at("state");
  response["metadata"] = session.at("metadata");
  return response;
}

Json AgentSessionManager::GetRecentSteps(const std::string& session_id,
                                         std::size_t limit) const {
  ValidateSessionId(session_id);

  const std::optional<Json> session = LoadSession(kv_, session_id);
  if (!session.has_value()) {
    return MakeNotFoundResponse("get_recent_steps", session_id);
  }

  Json response = MakeOkResponse("get_recent_steps", session_id);
  response["limit"] = limit;
  response["steps"] = SelectRecentEvents(session->at("events"), limit);
  return response;
}

Json AgentSessionManager::GetContext(const std::string& session_id,
                                     std::size_t limit) const {
  ValidateSessionId(session_id);

  const std::optional<Json> session = LoadSession(kv_, session_id);
  if (!session.has_value()) {
    return MakeNotFoundResponse("get_context", session_id);
  }

  Json response = MakeOkResponse("get_context", session_id);
  response["context"] = Json{
      {"session_id", session_id},
      {"metadata", session->at("metadata")},
      {"state", session->at("state")},
      {"recent_steps", SelectRecentEvents(session->at("events"), limit)},
  };
  return response;
}

Json AgentSessionManager::Replay(const std::string& session_id) const {
  ValidateSessionId(session_id);

  const std::optional<Json> session = LoadSession(kv_, session_id);
  if (!session.has_value()) {
    return MakeNotFoundResponse("replay", session_id);
  }

  Json replayed_state = EmptyState();

  // Rebuild state from scratch by replaying each stored diff in sequence.
  for (const Json& event : session->at("events")) {
    if (!event.is_object() || !event.contains("state_diff")) {
      throw std::invalid_argument("stored session event is invalid: " +
                                  session_id);
    }
    replayed_state = ApplyStateDiff(replayed_state, event.at("state_diff"));
  }

  Json response = MakeOkResponse("replay", session_id);
  response["events_replayed"] = session->at("events").size();
  response["replayed_state"] = replayed_state;
  response["current_state"] = session->at("state");
  return response;
}

}  // namespace agent
}  // namespace kv

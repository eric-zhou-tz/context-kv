#include "command/command.h"

#include <cstddef>
#include <optional>
#include <stdexcept>
#include <string>

#include "agent/session.h"

namespace kv {
namespace command {

namespace {

/**
 * @brief Builds a standard success response for commands that target one key.
 *
 * @param action Executed action name.
 * @param key Resolved KV key targeted by the action.
 * @return JSON response containing the success contract fields.
 */
Json MakeOkResponse(const std::string& action, const std::string& key) {
  return Json{
      {"ok", true},
      {"action", action},
      {"key", key},
  };
}

/**
 * @brief Builds a standard not-found response for read-style commands.
 *
 * @param action Executed action name.
 * @param key Resolved KV key targeted by the action.
 * @return JSON response describing a missing key lookup.
 */
Json MakeNotFoundResponse(const std::string& action, const std::string& key) {
  return Json{
      {"ok", false},
      {"action", action},
      {"key", key},
      {"error", "not found"},
  };
}

/**
 * @brief Returns a required parameter from the request.
 *
 * @param params Action parameters object.
 * @param field Required field name.
 * @return Referenced JSON field.
 * @throws std::invalid_argument When the field is missing.
 */
const Json& RequireParam(const Json& params, const std::string& field) {
  if (!params.contains(field)) {
    throw std::invalid_argument("request.params." + field + " is required");
  }
  return params.at(field);
}

/**
 * @brief Returns a required string parameter from the request.
 *
 * @param params Action parameters object.
 * @param field Required string field name.
 * @return Extracted string value.
 * @throws std::invalid_argument When the field is missing or not a string.
 */
std::string RequireString(const Json& params, const std::string& field) {
  const Json& value = RequireParam(params, field);
  if (!value.is_string()) {
    throw std::invalid_argument("request.params." + field +
                                " must be a string");
  }
  return value.get<std::string>();
}

/**
 * @brief Reads an object parameter with a default of `{}`.
 *
 * @param params Action parameters object.
 * @param field Field name to read.
 * @return Object field value or `{}` when absent.
 * @throws std::invalid_argument When a present field is not an object.
 */
Json GetOptionalObject(const Json& params, const std::string& field) {
  if (!params.contains(field)) {
    return Json::object();
  }

  const Json& value = params.at(field);
  if (!value.is_object()) {
    throw std::invalid_argument("request.params." + field +
                                " must be a JSON object");
  }
  return value;
}

/**
 * @brief Serializes a direct KV value for the generic put action.
 *
 * Strings are stored verbatim, while all other JSON values are stored as their
 * serialized JSON representation.
 *
 * @param value Request value to store.
 * @return Value string to hand to the generic KV layer.
 */
std::string SerializePutValue(const Json& value) {
  return value.is_string() ? value.get<std::string>() : value.dump();
}

/**
 * @brief Returns an optional non-negative integer parameter.
 *
 * @param params Action parameters object.
 * @param field Optional field name.
 * @param default_value Value returned when the field is absent.
 * @return Parsed non-negative integer value.
 * @throws std::invalid_argument When the field is present but invalid.
 */
std::size_t GetOptionalLimit(const Json& params, const std::string& field,
                             std::size_t default_value) {
  if (!params.contains(field)) {
    return default_value;
  }

  const Json& value = params.at(field);
  if (value.is_number_unsigned()) {
    return value.get<std::size_t>();
  }
  if (value.is_number_integer()) {
    const long long count = value.get<long long>();
    if (count >= 0) {
      return static_cast<std::size_t>(count);
    }
  }

  throw std::invalid_argument("request.params." + field +
                              " must be a non-negative integer");
}

}  // namespace

Json execute_command(const Json& request, store::KVStore& kv) {
  const std::string action = request.at("action").get<std::string>();
  const Json params = request.value("params", Json::object());
  agent::AgentSessionManager agent(kv);

  if (action == "begin_session") {
    return agent.BeginSession(GetOptionalObject(params, "initial_state"));
  }

  if (action == "get_state") {
    return agent.GetState(RequireString(params, "session_id"));
  }

  if (action == "update_state") {
    return agent.UpdateState(RequireString(params, "session_id"),
                             RequireParam(params, "state_diff"));
  }

  if (action == "get_recent_steps") {
    return agent.GetRecentSteps(RequireString(params, "session_id"),
                                GetOptionalLimit(params, "limit", 5));
  }

  if (action == "get_context") {
    return agent.GetContext(RequireString(params, "session_id"),
                            GetOptionalLimit(params, "limit", 5));
  }

  if (action == "replay") {
    return agent.Replay(RequireString(params, "session_id"));
  }

  if (action == "log_step") {
    return agent.LogStep(RequireString(params, "session_id"),
                         RequireString(params, "action"),
                         GetOptionalObject(params, "input"),
                         GetOptionalObject(params, "output"),
                         RequireParam(params, "state_diff"));
  }

  if (action == "put") {
    const std::string key = RequireString(params, "key");
    const Json& value = RequireParam(params, "value");
    kv.Set(key, SerializePutValue(value));
    return MakeOkResponse(action, key);
  }

  if (action == "get") {
    const std::string key = RequireString(params, "key");
    const std::optional<std::string> value = kv.Get(key);
    if (!value.has_value()) {
      return MakeNotFoundResponse(action, key);
    }

    Json response = MakeOkResponse(action, key);
    response["value"] = *value;
    return response;
  }

  if (action == "delete") {
    const std::string key = RequireString(params, "key");
    kv.Delete(key);
    return MakeOkResponse(action, key);
  }

  throw std::invalid_argument("unsupported action: " + action);
}

}  // namespace command
}  // namespace kv

#include "parser/command_parser.h"

#include <stdexcept>

namespace kv {
namespace parser {

namespace {

/**
 * @brief Validates the high-level shape of a parsed agent request.
 *
 * The parser is responsible only for request framing. Action-specific
 * semantics remain in the command layer.
 *
 * @param request Parsed JSON value.
 * @throws std::invalid_argument When the request shape is invalid.
 */
void ValidateRequestShape(const Json& request) {
  if (!request.is_object()) {
    throw std::invalid_argument("request must be a JSON object");
  }
  if (!request.contains("action")) {
    throw std::invalid_argument("request.action is required");
  }
  if (!request.at("action").is_string()) {
    throw std::invalid_argument("request.action must be a string");
  }
  if (request.contains("params") && !request.at("params").is_object()) {
    throw std::invalid_argument("request.params must be an object");
  }
}

}  // namespace

Json parse_agent_request(const std::string& raw) {
  try {
    Json request = Json::parse(raw);
    ValidateRequestShape(request);
    return request;
  } catch (const Json::parse_error&) {
    throw std::invalid_argument("request must be valid JSON");
  }
}

}  // namespace parser
}  // namespace kv

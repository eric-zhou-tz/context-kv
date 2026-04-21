#ifndef KV_STORE_PARSER_COMMAND_PARSER_H_
#define KV_STORE_PARSER_COMMAND_PARSER_H_

#include <string>

namespace kv {
namespace parser {

/**
 * @brief Enumerates the supported command kinds.
 */
enum class CommandType {
  kInvalid,
  kSet,
  kGet,
  kDel,
  kClearPersistence,
  kHelp,
  kExit,
};

/**
 * @brief Represents a parsed command and its arguments.
 */
struct Command {
  /** @brief Parsed command kind. */
  CommandType type = CommandType::kInvalid;
  /** @brief Key argument for commands that target a key. */
  std::string key;
  /** @brief Value argument for commands that store data. */
  std::string value;
  /** @brief Parser error message for invalid commands. */
  std::string error_message;

  /**
   * @brief Checks whether the command parsed successfully.
   *
   * @return `true` when the command type is not `kInvalid`.
   */
  bool IsValid() const;
};

/**
 * @brief Parses raw CLI input into structured commands.
 */
class CommandParser {
 public:
  /**
   * @brief Parses a single line of user input.
   *
   * @param input Raw command line entered by the user.
   * @return Parsed command description or an invalid command with an error.
   */
  Command Parse(const std::string& input) const;
};

}  // namespace parser
}  // namespace kv

#endif  // KV_STORE_PARSER_COMMAND_PARSER_H_

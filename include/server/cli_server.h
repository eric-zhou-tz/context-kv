#ifndef KV_STORE_SERVER_CLI_SERVER_H_
#define KV_STORE_SERVER_CLI_SERVER_H_

#include <iosfwd>

#include "parser/command_parser.h"
#include "store/kv_store.h"

namespace kv {
namespace server {

/**
 * @brief Runs the interactive command-line interface for the key-value store.
 */
class CliServer {
 public:
  /**
   * @brief Constructs a CLI server using external parser and store components.
   *
   * @param parser Parser used to convert raw input into commands.
   * @param store Store used to execute parsed commands.
   */
  CliServer(parser::CommandParser& parser, store::KVStore& store);

  /**
   * @brief Starts the read-evaluate-print loop.
   *
   * @param input Stream to read command lines from.
   * @param output Stream to write prompts and responses to.
   */
  void Run(std::istream& input, std::ostream& output);

 private:
  /**
   * @brief Executes a parsed command and writes the result.
   *
   * @param command Parsed command to execute.
   * @param output Stream used for command responses.
   * @param running Loop control flag updated by commands such as `EXIT`.
   */
  void Execute(const parser::Command& command, std::ostream& output, bool& running);

  /**
   * @brief Prints the list of supported commands.
   *
   * @param output Stream used for help text.
   */
  void PrintHelp(std::ostream& output) const;

  /** @brief Parser dependency used to decode user input. */
  parser::CommandParser& parser_;
  /** @brief Store dependency used to service parsed commands. */
  store::KVStore& store_;
};

}  // namespace server
}  // namespace kv

#endif  // KV_STORE_SERVER_CLI_SERVER_H_

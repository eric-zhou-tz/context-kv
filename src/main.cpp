#include <iostream>

#include "parser/command_parser.h"
#include "server/cli_server.h"
#include "store/kv_store.h"

/**
 * @brief Creates the application components and starts the CLI loop.
 *
 * @return Exit status code for the operating system.
 */
int main() {
  kv::store::KVStore store;
  kv::parser::CommandParser parser;
  kv::server::CliServer server(parser, store);

  server.Run(std::cin, std::cout);
  return 0;
}

#include <cassert>
#include <sstream>
#include <string>

#include "parser/command_parser.h"
#include "server/cli_server.h"
#include "store/kv_store.h"

int main() {
  kv::store::KVStore store;
  store.Set("alpha", "1");
  assert(store.Contains("alpha"));
  assert(store.Get("alpha").value() == "1");
  assert(store.Delete("alpha"));
  assert(!store.Get("alpha").has_value());

  kv::parser::CommandParser parser;
  const kv::parser::Command set_command = parser.Parse("SET project kv_store");
  assert(set_command.IsValid());
  assert(set_command.type == kv::parser::CommandType::kSet);
  assert(set_command.key == "project");
  assert(set_command.value == "kv_store");

  std::istringstream input("SET name codex\nGET name\nDEL name\nGET name\nEXIT\n");
  std::ostringstream output;
  kv::server::CliServer server(parser, store);
  server.Run(input, output);

  const std::string transcript = output.str();
  assert(transcript.find("OK") != std::string::npos);
  assert(transcript.find("codex") != std::string::npos);
  assert(transcript.find("(nil)") != std::string::npos);
  assert(transcript.find("Bye") != std::string::npos);

  return 0;
}

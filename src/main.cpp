#include <iostream>

#include "parser/command_parser.h"
#include "persistence/snapshot.h"
#include "persistence/wal.h"
#include "server/cli_server.h"
#include "store/kv_store.h"

/**
 * @brief Creates the application components and starts the CLI loop.
 *
 * @return Exit status code for the operating system.
 */
int main() {
  // Wire persistence into the store before replay so subsequent CLI mutations
  // are durably logged.
  kv::persistence::WriteAheadLog wal;
  kv::persistence::Snapshot snapshot;
  kv::store::KVStore store(&wal, &snapshot);

  // Recovery is layered: load the last full checkpoint first, then replay only
  // WAL records written after the byte offset covered by that snapshot.
  std::cout << "Loading snapshot...\n";
  const kv::persistence::SnapshotLoadResult snapshot_result =
      store.LoadSnapshot(snapshot);
  std::cout << "Loaded " << snapshot_result.entry_count
            << " snapshot entrie(s)\n";

  std::cout << "Replaying WAL...\n";
  const std::size_t recovered_operations =
      store.ReplayFromWal(wal, snapshot_result.wal_offset);
  std::cout << "Recovered " << recovered_operations << " operation(s)\n";

  kv::parser::CommandParser parser;
  kv::server::CliServer server(parser, store);

  server.Run(std::cin, std::cout);
  return 0;
}

#include <filesystem>
#include <iostream>
#include <stdexcept>
#include <string>
#include <unistd.h>

#include "command/command.h"
#include "parser/command_parser.h"
#include "persistence/snapshot.h"
#include "persistence/wal.h"
#include "store/kv_store.h"

namespace {

using Json = kv::parser::Json;

constexpr char kDefaultDbDirectory[] = "data";

/**
 * @brief Runtime configuration for the single-request agent pipeline.
 */
struct MainOptions {
  /** @brief Directory that stores WAL and snapshot files. */
  std::string db_directory;
};

/**
 * @brief Parses supported command-line flags for the program entry point.
 *
 * Supported flags:
 * - `--db <directory>`: directory used for WAL and snapshot files
 *
 * @param argc Argument count from `main`.
 * @param argv Argument vector from `main`.
 * @return Parsed runtime options.
 * @throws std::invalid_argument When flags are missing required values or are unknown.
 */
MainOptions ParseMainOptions(int argc, char* argv[]) {
  MainOptions options;

  for (int index = 1; index < argc; ++index) {
    const std::string argument = argv[index];
    if (argument == "--db") {
      if (index + 1 >= argc) {
        throw std::invalid_argument("--db requires a directory path");
      }
      options.db_directory = argv[++index];
      continue;
    }

    throw std::invalid_argument("unknown argument: " + argument);
  }

  return options;
}

/**
 * @brief Returns the WAL path for the configured database directory.
 *
 * @param options Parsed runtime options.
 * @return Filesystem path to the WAL file.
 */
std::string BuildWalPath(const MainOptions& options) {
  const std::filesystem::path db_root =
      options.db_directory.empty() ? std::filesystem::path(kDefaultDbDirectory)
                                   : std::filesystem::path(options.db_directory);
  return (db_root / "kv_store.wal")
      .string();
}

/**
 * @brief Returns the snapshot path for the configured database directory.
 *
 * @param options Parsed runtime options.
 * @return Filesystem path to the snapshot file.
 */
std::string BuildSnapshotPath(const MainOptions& options) {
  const std::filesystem::path db_root =
      options.db_directory.empty() ? std::filesystem::path(kDefaultDbDirectory)
                                   : std::filesystem::path(options.db_directory);
  return (db_root / "kv_store.snapshot")
      .string();
}

/**
 * @brief Ensures the persistence directory exists before opening files.
 *
 * @param options Parsed runtime options.
 */
void EnsureDbDirectory(const MainOptions& options) {
  const std::filesystem::path db_root =
      options.db_directory.empty() ? std::filesystem::path(kDefaultDbDirectory)
                                   : std::filesystem::path(options.db_directory);
  std::filesystem::create_directories(db_root);
}

/**
 * @brief Builds a JSON error response for entry-point failures.
 *
 * @param message Failure description to return to the caller.
 * @return JSON object describing the failure.
 */
Json MakeErrorResponse(const std::string& message) {
  return Json{
      {"ok", false},
      {"error", message},
  };
}

/**
 * @brief Returns whether stdin is attached to an interactive terminal.
 *
 * @return `true` when stdin is a TTY, otherwise `false`.
 */
bool IsInteractiveInput() {
  return ::isatty(STDIN_FILENO) != 0;
}

/**
 * @brief Parses and executes one raw JSON request against the KV store.
 *
 * @param raw Raw JSON request string.
 * @param store KV store used for command execution.
 * @return JSON response from the command layer.
 */
Json ExecuteRawRequest(const std::string& raw, kv::store::KVStore& store) {
  const Json request = kv::parser::parse_agent_request(raw);
  return kv::command::execute_command(request, store);
}

/**
 * @brief Prints lightweight interactive usage for the JSON command shell.
 *
 * @param output Stream used for help text.
 */
void PrintInteractiveHelp(std::ostream& output) {
  output << "Enter one JSON request per line.\n";
  output << "Commands:\n";
  output << "  help  Show this message\n";
  output << "  quit  Exit the shell\n";
  output << "Example:\n";
  output << "  {\"action\":\"put\",\"params\":{\"key\":\"x\",\"value\":\"y\"}}\n";
}

/**
 * @brief Runs a lightweight interactive shell for manual JSON request testing.
 *
 * This shell accepts only raw JSON requests plus `help` and `quit`. Each valid
 * request is routed through the same parser and command path as agent input.
 *
 * @param input Stream used to read terminal lines.
 * @param output Stream used to print prompts and responses.
 * @param store KV store used for command execution.
 * @return Exit status code for the operating system.
 */
int RunInteractiveShell(std::istream& input, std::ostream& output,
                        kv::store::KVStore& store) {
  PrintInteractiveHelp(output);

  std::string raw;
  while (true) {
    output << "agentkv> ";
    output.flush();

    if (!std::getline(input, raw)) {
      output << '\n';
      return 0;
    }

    if (raw == "quit") {
      return 0;
    }
    if (raw == "help") {
      PrintInteractiveHelp(output);
      continue;
    }
    if (raw.empty()) {
      continue;
    }

    try {
      output << ExecuteRawRequest(raw, store).dump() << std::endl;
    } catch (const std::exception& error) {
      output << MakeErrorResponse(error.what()).dump() << std::endl;
    }
  }
}

}  // namespace

/**
 * @brief Executes agent JSON requests through the parser and command pipeline.
 *
 * The program initializes persistence, replays durable state, and then either
 * reads one raw JSON request from piped stdin or starts a lightweight
 * interactive shell when launched from a terminal.
 *
 * @param argc Argument count supplied by the operating system.
 * @param argv Argument vector supplied by the operating system.
 * @return Exit status code for the operating system.
 */
int main(int argc, char* argv[]) {
  try {
    const MainOptions options = ParseMainOptions(argc, argv);
    EnsureDbDirectory(options);

    // Load persistence
    kv::persistence::WriteAheadLog wal(BuildWalPath(options));
    kv::persistence::Snapshot snapshot(BuildSnapshotPath(options));
    kv::store::KVStore store(&wal, &snapshot);

    const kv::persistence::SnapshotLoadResult snapshot_result =
        store.LoadSnapshot(snapshot);
    store.ReplayFromWal(wal, snapshot_result.wal_offset);

    if (IsInteractiveInput()) {
      return RunInteractiveShell(std::cin, std::cout, store);
    }

    std::string raw;
    std::getline(std::cin, raw);
    std::cout << ExecuteRawRequest(raw, store).dump() << std::endl;
    return 0;
  } catch (const std::exception& error) {
    std::cout << MakeErrorResponse(error.what()).dump() << std::endl;
    return 1;
  }
}

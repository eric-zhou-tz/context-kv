#include <cassert>
#include <cstdio>
#include <cstdint>
#include <fstream>
#include <stdexcept>
#include <sstream>
#include <string>
#include <unordered_map>

#include "parser/command_parser.h"
#include "persistence/snapshot.h"
#include "persistence/wal.h"
#include "server/cli_server.h"
#include "store/kv_store.h"

namespace {

template <typename T>
void WritePrimitive(std::ofstream& output, const T& value) {
  output.write(reinterpret_cast<const char*>(&value), sizeof(T));
}

void AppendUnknownWalRecord(const std::string& path) {
  // Unknown operations are framed correctly so replay can skip them and keep
  // reading later records.
  std::ofstream output(path, std::ios::binary | std::ios::app);
  const std::uint32_t record_length = sizeof(std::uint8_t);
  const std::uint8_t op = 99;
  WritePrimitive(output, record_length);
  WritePrimitive(output, op);
}

void AppendTruncatedWalRecord(const std::string& path) {
  // The length promises more bytes than are written, simulating a crash during
  // the final WAL append.
  std::ofstream output(path, std::ios::binary | std::ios::app);
  const std::uint32_t record_length = 8;
  const std::uint8_t op = 1;
  WritePrimitive(output, record_length);
  WritePrimitive(output, op);
}

}  // namespace

int main() {
  // Basic in-memory store behavior without persistence.
  kv::store::KVStore store;
  store.Set("alpha", "1");
  assert(store.Contains("alpha"));
  assert(store.Get("alpha").value() == "1");
  assert(store.Delete("alpha"));
  assert(!store.Get("alpha").has_value());

  // Parser accepts command aliases and preserves SET values.
  kv::parser::CommandParser parser;
  const kv::parser::Command set_command = parser.Parse("SET project kv_store");
  assert(set_command.IsValid());
  assert(set_command.type == kv::parser::CommandType::kSet);
  assert(set_command.key == "project");
  assert(set_command.value == "kv_store");

  const kv::parser::Command delete_command = parser.Parse("DELETE project");
  assert(delete_command.IsValid());
  assert(delete_command.type == kv::parser::CommandType::kDel);
  assert(delete_command.key == "project");

  const kv::parser::Command clear_persistence_command =
      parser.Parse("CLEAR PERSISTENCE");
  assert(clear_persistence_command.IsValid());
  assert(clear_persistence_command.type ==
         kv::parser::CommandType::kClearPersistence);

  // Exercise the CLI loop through streams so the test stays deterministic.
  std::istringstream input("SET name codex\nGET name\nDEL name\nGET name\nEXIT\n");
  std::ostringstream output;
  kv::server::CliServer server(parser, store);
  server.Run(input, output);

  const std::string transcript = output.str();
  assert(transcript.find("OK") != std::string::npos);
  assert(transcript.find("codex") != std::string::npos);
  assert(transcript.find("(nil)") != std::string::npos);
  assert(transcript.find("Bye") != std::string::npos);

  const std::string wal_path = "/tmp/kv_store_wal_test.log";
  std::remove(wal_path.c_str());

  // Write a real WAL through the store, including a value with spaces and a
  // delete that must survive replay.
  {
    kv::persistence::WriteAheadLog wal(wal_path);
    kv::store::KVStore logged_store(&wal);
    logged_store.Set("alpha", "1");
    logged_store.Set("message", "hello world");
    assert(logged_store.Delete("alpha"));
  }

  // Reopen the WAL to mimic a process restart.
  {
    kv::persistence::WriteAheadLog wal(wal_path);
    kv::store::KVStore recovered_store(&wal);
    const std::size_t recovered_operations = recovered_store.ReplayFromWal(wal);
    assert(recovered_operations == 3);
    assert(!recovered_store.Contains("alpha"));
    assert(recovered_store.Get("message").value() == "hello world");
  }

  std::remove(wal_path.c_str());

  // Replay should count and apply valid records while skipping malformed
  // bounded records.
  {
    kv::persistence::WriteAheadLog wal(wal_path);
    wal.append_set("good", "value");
  }

  AppendUnknownWalRecord(wal_path);

  {
    kv::persistence::WriteAheadLog wal(wal_path);
    wal.append_delete("absent");
  }

  AppendTruncatedWalRecord(wal_path);

  // The truncated trailing record stops recovery after the earlier valid
  // records.
  {
    kv::persistence::WriteAheadLog wal(wal_path);
    std::unordered_map<std::string, std::string> recovered;
    const std::size_t recovered_operations = wal.replay(recovered);
    assert(recovered_operations == 2);
    assert(recovered["good"] == "value");
    assert(recovered.find("absent") == recovered.end());
  }

  std::remove(wal_path.c_str());

  const std::string snapshot_path = "/tmp/kv_store_snapshot_test.data";
  std::remove(snapshot_path.c_str());
  std::remove((snapshot_path + ".tmp").c_str());

  // Snapshots save the full materialized map and replace the target on success.
  {
    kv::persistence::Snapshot snapshot(snapshot_path);
    std::unordered_map<std::string, std::string> original;
    original["alpha"] = "1";
    original["empty"] = "";
    original["message"] = "hello world";

    snapshot.save(original);

    std::unordered_map<std::string, std::string> loaded;
    const kv::persistence::SnapshotLoadResult result = snapshot.load(loaded);
    assert(result.loaded);
    assert(result.entry_count == original.size());
    assert(result.wal_offset == 0);
    assert(loaded == original);
  }

  // Legacy snapshots without metadata still load and fall back to WAL offset 0.
  {
    std::remove(snapshot_path.c_str());
    std::ofstream output(snapshot_path, std::ios::binary | std::ios::trunc);
    const std::uint32_t entry_count = 1;
    const std::uint32_t key_size = 6;
    const std::uint32_t value_size = 6;
    WritePrimitive(output, entry_count);
    WritePrimitive(output, key_size);
    output.write("legacy", key_size);
    WritePrimitive(output, value_size);
    output.write("format", value_size);
  }

  {
    kv::persistence::Snapshot snapshot(snapshot_path);
    std::unordered_map<std::string, std::string> loaded;
    const kv::persistence::SnapshotLoadResult result = snapshot.load(loaded);
    assert(result.loaded);
    assert(result.entry_count == 1);
    assert(result.wal_offset == 0);
    assert(loaded["legacy"] == "format");
  }

  // KVStore checkpoints through Snapshot after the configured write interval.
  {
    const std::string checkpoint_wal_path = "/tmp/kv_store_checkpoint_wal.log";
    std::remove(snapshot_path.c_str());
    std::remove((snapshot_path + ".tmp").c_str());
    std::remove(checkpoint_wal_path.c_str());

    kv::persistence::Snapshot snapshot(snapshot_path);
    {
      kv::persistence::WriteAheadLog wal(checkpoint_wal_path);
      kv::store::KVStore checkpointed_store(&wal, &snapshot);
      for (int i = 0; i < 1000; ++i) {
        checkpointed_store.Set("key" + std::to_string(i),
                               "value" + std::to_string(i));
      }
      checkpointed_store.Set("after_checkpoint", "value");
    }

    std::unordered_map<std::string, std::string> loaded;
    const kv::persistence::SnapshotLoadResult result = snapshot.load(loaded);
    assert(result.loaded);
    assert(result.entry_count == 1000);
    assert(result.wal_offset > 0);
    assert(loaded["key0"] == "value0");
    assert(loaded["key999"] == "value999");
    assert(loaded.find("after_checkpoint") == loaded.end());

    {
      kv::persistence::WriteAheadLog wal(checkpoint_wal_path);
      const std::size_t recovered_operations =
          wal.replay_from(result.wal_offset, loaded);
      assert(recovered_operations == 1);
      assert(loaded["after_checkpoint"] == "value");
    }

    std::remove(checkpoint_wal_path.c_str());
  }

  // Startup recovery loads a snapshot first, then applies only later WAL bytes.
  {
    const std::string recovery_wal_path = "/tmp/kv_store_recovery_wal.log";
    std::remove(snapshot_path.c_str());
    std::remove((snapshot_path + ".tmp").c_str());
    std::remove(recovery_wal_path.c_str());

    kv::persistence::Snapshot snapshot(snapshot_path);
    std::unordered_map<std::string, std::string> checkpoint;
    checkpoint["old"] = "snapshot";
    checkpoint["keep"] = "snapshot";

    {
      kv::persistence::WriteAheadLog wal(recovery_wal_path);
      wal.append_set("only_old_wal", "must be skipped");
      const std::uint64_t snapshot_offset = wal.current_offset();
      snapshot.save(checkpoint, snapshot_offset);
      wal.append_delete("old");
      wal.append_set("keep", "wal");
      wal.append_set("new", "wal");
    }

    kv::persistence::WriteAheadLog wal(recovery_wal_path);
    kv::store::KVStore recovered_store(&wal, &snapshot);
    const kv::persistence::SnapshotLoadResult snapshot_result =
        recovered_store.LoadSnapshot(snapshot);
    const std::size_t recovered_operations =
        recovered_store.ReplayFromWal(wal, snapshot_result.wal_offset);

    assert(snapshot_result.loaded);
    assert(snapshot_result.entry_count == 2);
    assert(snapshot_result.wal_offset > 0);
    assert(recovered_operations == 3);
    assert(!recovered_store.Contains("only_old_wal"));
    assert(!recovered_store.Contains("old"));
    assert(recovered_store.Get("keep").value() == "wal");
    assert(recovered_store.Get("new").value() == "wal");

    std::remove(recovery_wal_path.c_str());
  }

  // The CLI can clear WAL and snapshot files while keeping the live store usable.
  {
    const std::string clear_wal_path = "/tmp/kv_store_clear_wal.log";
    std::remove(snapshot_path.c_str());
    std::remove((snapshot_path + ".tmp").c_str());
    std::remove(clear_wal_path.c_str());

    kv::persistence::Snapshot snapshot(snapshot_path);
    std::unordered_map<std::string, std::string> snapshotted;
    snapshotted["from_snapshot"] = "old";
    snapshot.save(snapshotted);

    {
      kv::persistence::WriteAheadLog wal(clear_wal_path);
      kv::store::KVStore persistent_store(&wal, &snapshot);
      persistent_store.Set("before_clear", "old");

      std::istringstream clear_input("CLEAR PERSISTENCE\nSET after_clear new\nEXIT\n");
      std::ostringstream clear_output;
      kv::server::CliServer clear_server(parser, persistent_store);
      clear_server.Run(clear_input, clear_output);

      assert(clear_output.str().find("OK persistence cleared") !=
             std::string::npos);
      assert(persistent_store.Get("before_clear").value() == "old");
      assert(persistent_store.Get("after_clear").value() == "new");
    }

    std::unordered_map<std::string, std::string> loaded_snapshot;
    const kv::persistence::SnapshotLoadResult cleared_snapshot =
        snapshot.load(loaded_snapshot);
    assert(!cleared_snapshot.loaded);
    assert(cleared_snapshot.entry_count == 0);
    assert(cleared_snapshot.wal_offset == 0);

    {
      kv::persistence::WriteAheadLog wal(clear_wal_path);
      std::unordered_map<std::string, std::string> replayed;
      const std::size_t recovered_operations = wal.replay(replayed);
      assert(recovered_operations == 1);
      assert(replayed.find("before_clear") == replayed.end());
      assert(replayed["after_clear"] == "new");
    }

    std::remove(clear_wal_path.c_str());
  }

  // A truncated snapshot must not partially replace the caller's map.
  {
    std::ofstream output(snapshot_path, std::ios::binary | std::ios::trunc);
    const std::uint32_t entry_count = 1;
    WritePrimitive(output, entry_count);
  }

  {
    kv::persistence::Snapshot snapshot(snapshot_path);
    std::unordered_map<std::string, std::string> existing;
    existing["keep"] = "me";

    bool threw = false;
    try {
      snapshot.load(existing);
    } catch (const std::runtime_error&) {
      threw = true;
    }

    assert(threw);
    assert(existing.size() == 1);
    assert(existing["keep"] == "me");
  }

  std::remove(snapshot_path.c_str());
  std::remove((snapshot_path + ".tmp").c_str());

  return 0;
}

# Design Notes

## Overview

Phase 1 is a small in-memory key-value store with a command-line interface.
The implementation is intentionally simple, but the directory layout and module
boundaries are chosen to support later systems work.

## Module Boundaries

### `store`

- Owns key/value data and core mutation/query operations
- Contains no parsing or I/O logic
- Can later become the home for persistence-aware storage engines

### `parser`

- Translates raw command text into typed commands
- Keeps syntax validation outside the store
- Can later support richer protocols without changing storage code

### `server`

- Owns the interactive loop and command dispatch
- Depends on parser and store, but not vice versa
- Can later be replaced or extended by TCP/HTTP request handlers

### `common`

- Holds small, dependency-free helpers shared across modules
- Keeps low-level utility code out of business logic

## Why This Structure Scales

- The store API stays stable while interface layers evolve.
- New frontends can reuse the same store and parser components.
- Persistence can be added behind the store boundary without pushing file I/O
  concerns into the CLI or parser.
- Concurrency can be introduced around request handling and storage access later,
  after Phase 1 semantics are correct.

## Phase 1 Scope

Included:

- `SET key value`
- `GET key`
- `DEL key`
- `HELP`
- `EXIT`

Not included:

- Networking
- Persistence
- Transactions
- Replication
- Concurrency control

## Extension Path

1. Persistence
   - Add a write-ahead log module
   - Add snapshot/load support
   - Preserve the `KVStore` interface while changing internals

2. Networking
   - Add a socket server beside the CLI server
   - Reuse the same command dispatch flow

3. Concurrency
   - Introduce synchronization at the store boundary
   - Add a worker model in the server layer
   - Preserve parsing and command execution semantics

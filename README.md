# KV Store (Phase 1)

A clean C++ foundation for a single-threaded in-memory key-value store.

This repository is intentionally small in Phase 1, but it is structured to
evolve into a production-quality system with persistence, networking, and
concurrency in later phases.

## Goals

- Keep the Phase 1 implementation simple and readable.
- Maintain clean boundaries between parsing, storage, and interface layers.
- Make future extensions possible without rewriting the core store logic.
- Avoid external dependencies and keep the build portable.

## Features

- In-memory `KVStore` backed by `std::unordered_map`
- Basic CLI with `SET`, `GET`, `DEL`, `HELP`, and `EXIT`
- Small parser module isolated from storage logic
- Simple Makefile-based build
- Docker-friendly project layout
- No external libraries

## Directory Layout

```text
.
├── Dockerfile
├── DESIGN.md
├── Makefile
├── README.md
├── include
│   ├── common
│   │   └── string_utils.h
│   ├── parser
│   │   └── command_parser.h
│   ├── server
│   │   └── cli_server.h
│   └── store
│       └── kv_store.h
├── scripts
│   ├── build.sh
│   └── run.sh
├── src
│   ├── common
│   │   └── string_utils.cpp
│   ├── main.cpp
│   ├── parser
│   │   └── command_parser.cpp
│   ├── server
│   │   └── cli_server.cpp
│   └── store
│       └── kv_store.cpp
└── tests
    └── test_kv_store.cpp
```

## Build

```bash
make
```

## Run

```bash
./bin/kv_store
```

Example session:

```text
kv-store> SET language cpp
OK
kv-store> GET language
cpp
kv-store> DEL language
1
kv-store> EXIT
Bye
```

## Test

```bash
make test
```

## Docker

```bash
docker build -t kv-store .
docker run --rm -it kv-store
```

## Next Phases

- Add a storage engine abstraction for persistent backends
- Introduce WAL and snapshotting
- Add TCP request handling
- Add worker/threading model around the server layer
- Add richer test coverage and benchmarking

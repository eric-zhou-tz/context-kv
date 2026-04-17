#!/usr/bin/env bash

set -euo pipefail

make
exec ./bin/kv_store

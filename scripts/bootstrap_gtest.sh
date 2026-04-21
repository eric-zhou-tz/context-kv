#!/usr/bin/env bash
set -euo pipefail

ROOT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
GTEST_DIR="${ROOT_DIR}/external/googletest"

if [[ -d "${GTEST_DIR}/googletest" ]]; then
  echo "GoogleTest already exists at ${GTEST_DIR}"
  exit 0
fi

mkdir -p "${ROOT_DIR}/external"
git clone --depth 1 --branch v1.14.0 \
  https://github.com/google/googletest.git "${GTEST_DIR}"

echo "GoogleTest is ready. Run: make test"

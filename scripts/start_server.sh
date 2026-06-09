#!/usr/bin/env bash
set -euo pipefail

ROOT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
cd "$ROOT_DIR"

CONFIG_PATH="${1:-config/app.conf}"

fail() {
  echo "FAIL: $*" >&2
  exit 1
}

[[ -x build/oj_server ]] || fail "build/oj_server not found; run scripts/build.sh first"
[[ -f "$CONFIG_PATH" ]] || fail "config not found: $CONFIG_PATH"

exec ./build/oj_server "$CONFIG_PATH"

#!/usr/bin/env bash
set -euo pipefail

ROOT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
cd "$ROOT_DIR"

fail() {
  echo "FAIL: $*" >&2
  exit 1
}

command -v make >/dev/null || fail "make not found"
command -v g++ >/dev/null || fail "g++ not found"
command -v mysql_config >/dev/null || fail "mysql_config not found"

if [[ $# -gt 0 ]]; then
  make "$@"
else
  make all
fi

[[ -x build/oj_server ]] || fail "build/oj_server was not created"
echo "PASS: build/oj_server is ready"

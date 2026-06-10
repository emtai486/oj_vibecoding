#!/usr/bin/env bash
set -euo pipefail

ROOT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
cd "$ROOT_DIR"

MODE="full"
STRICT_OS=0
RUN_INIT_DB=1
CONFIG_PATH="config/app.conf"

usage() {
  cat <<'USAGE'
Usage:
  bash scripts/deploy_verify.sh [config_path]
  bash scripts/deploy_verify.sh --basic [config_path]
  bash scripts/deploy_verify.sh --strict-os [config_path]
  bash scripts/deploy_verify.sh --no-init-db [config_path]

Modes:
  full   Build, initialize DB, verify MySQL, static assets, and API workflows.
  basic  Build and verify local dependencies, C++ compile/run, and static assets.

Options:
  --strict-os   Fail unless /etc/os-release reports Ubuntu 22.04.
  --no-init-db  Do not import sql/schema.sql and sql/seed.sql before full checks.
USAGE
}

while [[ $# -gt 0 ]]; do
  case "$1" in
    --basic)
      MODE="basic"
      shift
      ;;
    --full)
      MODE="full"
      shift
      ;;
    --strict-os)
      STRICT_OS=1
      shift
      ;;
    --no-init-db)
      RUN_INIT_DB=0
      shift
      ;;
    --help|-h)
      usage
      exit 0
      ;;
    *)
      CONFIG_PATH="$1"
      shift
      ;;
  esac
done

fail() {
  echo "FAIL: $*" >&2
  exit 1
}

pass() {
  echo "PASS: $*"
}

warn() {
  echo "WARN: $*" >&2
}

config_value() {
  local key="$1"
  local default_value="$2"

  if [[ ! -f "$CONFIG_PATH" ]]; then
    echo "$default_value"
    return
  fi

  awk -F= -v key="$key" '
    $0 !~ /^[[:space:]]*#/ && $1 ~ "^[[:space:]]*" key "[[:space:]]*$" {
      value=$2
      sub(/^[[:space:]]*/, "", value)
      sub(/[[:space:]]*$/, "", value)
      print value
      found=1
      exit
    }
    END { if (!found) exit 1 }
  ' "$CONFIG_PATH" 2>/dev/null || echo "$default_value"
}

write_server_config() {
  local source_config="$1"
  local target_config="$2"
  local port="$3"

  awk -v port="$port" '
    BEGIN { wrote_host=0; wrote_port=0 }
    /^[[:space:]]*server\.host[[:space:]]*=/ {
      print "server.host=127.0.0.1"
      wrote_host=1
      next
    }
    /^[[:space:]]*server\.port[[:space:]]*=/ {
      print "server.port=" port
      wrote_port=1
      next
    }
    { print }
    END {
      if (!wrote_host) print "server.host=127.0.0.1"
      if (!wrote_port) print "server.port=" port
    }
  ' "$source_config" >"$target_config"
}

SERVER_PID=""
STARTED_BASE_URL=""
TMP_DIR="$(mktemp -d)"
trap 'if [[ -n "$SERVER_PID" ]]; then kill "$SERVER_PID" >/dev/null 2>&1 || true; wait "$SERVER_PID" >/dev/null 2>&1 || true; fi; rm -rf "$TMP_DIR"' EXIT

check_os() {
  if [[ ! -f /etc/os-release ]]; then
    [[ "$STRICT_OS" -eq 0 ]] || fail "/etc/os-release not found"
    warn "/etc/os-release not found; cannot verify Ubuntu 22.04"
    return
  fi

  # shellcheck disable=SC1091
  . /etc/os-release
  if [[ "${ID:-}" == "ubuntu" && "${VERSION_ID:-}" == "22.04" ]]; then
    pass "Ubuntu 22.04 environment detected"
    return
  fi

  local current="${PRETTY_NAME:-unknown Linux}"
  if [[ "$STRICT_OS" -eq 1 ]]; then
    fail "expected Ubuntu 22.04, got $current. This project currently treats the existing Ubuntu 22.04 server as the final acceptance environment."
  fi
  warn "expected Ubuntu 22.04 for final deployment verification; current environment is $current"
}

check_not_root() {
  if [[ "$(id -u)" == "0" ]]; then
    fail "service verification should not run as root"
  fi
  pass "not running as root"
}

check_dependencies() {
  command -v g++ >/dev/null || fail "g++ not found"
  command -v make >/dev/null || fail "make not found"
  command -v mysql >/dev/null || fail "mysql client not found"
  command -v mysql_config >/dev/null || fail "mysql_config not found"
  command -v python3 >/dev/null || fail "python3 not found"
  command -v curl >/dev/null || fail "curl not found"
  pass "required commands are available"
}

verify_cpp_compile_run() {
  local source="$TMP_DIR/hello.cpp"
  local binary="$TMP_DIR/hello"
  cat >"$source" <<'EOF'
#include <iostream>
int main() {
  std::cout << "cpp17 ok\n";
  return 0;
}
EOF

  g++ -std=c++17 "$source" -o "$binary"
  local output
  output="$("$binary")"
  [[ "$output" == "cpp17 ok" ]] || fail "unexpected g++ test output: $output"
  pass "g++ can compile and run C++17 code"
}

start_temp_server() {
  [[ -f "$CONFIG_PATH" ]] || fail "config not found: $CONFIG_PATH"
  [[ -x build/oj_server ]] || fail "build/oj_server not found"
  STARTED_BASE_URL=""

  local start_port="${OJ_DEPLOY_VERIFY_PORT:-18180}"
  for port in $(seq "$start_port" $((start_port + 40))); do
    local config_for_server="$TMP_DIR/app.$port.conf"
    local log_file="$TMP_DIR/server.$port.log"
    write_server_config "$CONFIG_PATH" "$config_for_server" "$port"
    ./build/oj_server "$config_for_server" >"$log_file" 2>&1 &
    SERVER_PID="$!"

    for _ in $(seq 1 30); do
      if curl -fsS "http://127.0.0.1:$port/health" >/dev/null 2>&1; then
        STARTED_BASE_URL="http://127.0.0.1:$port"
        return
      fi
      if ! kill -0 "$SERVER_PID" >/dev/null 2>&1; then
        break
      fi
      sleep 0.1
    done

    kill "$SERVER_PID" >/dev/null 2>&1 || true
    wait "$SERVER_PID" >/dev/null 2>&1 || true
    SERVER_PID=""

    if [[ "$port" -eq $((start_port + 40)) ]]; then
      cat "$TMP_DIR"/server.*.log >&2
      fail "server exited before becoming ready on ports $start_port-$((start_port + 40))"
    fi
  done
}

verify_static_assets() {
  start_temp_server

  curl -fsS "$STARTED_BASE_URL/index.html" | grep -F "<html" >/dev/null ||
    fail "index.html did not load through HTTP"
  curl -fsS "$STARTED_BASE_URL/problems.html" | grep -F "problem-list" >/dev/null ||
    fail "problems.html did not load through HTTP"
  curl -fsS "$STARTED_BASE_URL/js/api.js" | grep -F "getProblems()" >/dev/null ||
    fail "public/js/api.js did not load through HTTP"
  curl -fsS "$STARTED_BASE_URL/vendor/codemirror/codemirror.js" >/dev/null ||
    fail "CodeMirror asset did not load through HTTP"

  kill "$SERVER_PID" >/dev/null 2>&1 || true
  wait "$SERVER_PID" >/dev/null 2>&1 || true
  SERVER_PID=""
  STARTED_BASE_URL=""
  pass "frontend static assets load through local HTTP server"
}

check_os
check_not_root
check_dependencies

bash scripts/build.sh
verify_cpp_compile_run

if [[ "$MODE" == "basic" ]]; then
  cp config/app.example.conf "$TMP_DIR/basic.conf"
  CONFIG_PATH="$TMP_DIR/basic.conf"
  verify_static_assets
  echo "SKIP: DB-backed deployment checks were not run in --basic mode"
  exit 0
fi

[[ -f "$CONFIG_PATH" ]] || fail "config not found: $CONFIG_PATH"

if [[ "$RUN_INIT_DB" -eq 1 ]]; then
  bash scripts/init_db.sh "$CONFIG_PATH"
fi

./build/oj_server --check-db "$CONFIG_PATH"
pass "MySQL connection check passed"

verify_static_assets

python3 scripts/api_python_test.py "$CONFIG_PATH"
pass "ordinary user, admin, and submit API workflows passed"

echo "PASS: deployment verification completed"

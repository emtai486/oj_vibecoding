#!/usr/bin/env bash
set -euo pipefail

MODE="full"
CONFIG_PATH="config/app.conf"
BASE_URL="${OJ_API_BASE_URL:-}"
START_SERVER=1
CONFIG_FOR_SERVER=""

usage() {
  cat <<'USAGE'
Usage:
  bash scripts/api_curl_test.sh [config_path]
  bash scripts/api_curl_test.sh --basic [config_path]
  OJ_API_BASE_URL=http://127.0.0.1:8080 bash scripts/api_curl_test.sh --no-start

Modes:
  full   Verify all current APIs documented in API.md. Requires MySQL seed data.
  basic  Verify APIs that do not require a working database.

Environment:
  OJ_API_BASE_URL  Use an already running server instead of deriving URL from config.
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
    --no-start)
      START_SERVER=0
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

ROOT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
cd "$ROOT_DIR"

SERVER_PID=""
TMP_DIR="$(mktemp -d)"
COOKIE_JAR="$TMP_DIR/cookies.txt"
ADMIN_COOKIE_JAR="$TMP_DIR/admin-cookies.txt"
HEADERS_FILE="$TMP_DIR/headers.txt"
BODY_FILE="$TMP_DIR/body.json"
trap 'if [[ -n "$SERVER_PID" ]]; then kill "$SERVER_PID" >/dev/null 2>&1 || true; wait "$SERVER_PID" >/dev/null 2>&1 || true; fi; rm -rf "$TMP_DIR"' EXIT

fail() {
  echo "FAIL: $*" >&2
  exit 1
}

pass() {
  echo "PASS: $*"
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

if [[ -z "$BASE_URL" ]]; then
  HOST="$(config_value server.host 127.0.0.1)"
  PORT="$(config_value server.port 8080)"
  if [[ "$HOST" == "0.0.0.0" ]]; then
    HOST="127.0.0.1"
  fi
  BASE_URL="http://$HOST:$PORT"
fi

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

if [[ "$START_SERVER" -eq 1 ]]; then
  [[ -f "$CONFIG_PATH" ]] || fail "config not found: $CONFIG_PATH"
  [[ -x build/oj_server ]] || fail "build/oj_server not found; run make first"

  START_PORT="${OJ_API_TEST_PORT:-18080}"
  for port in $(seq "$START_PORT" $((START_PORT + 40))); do
    CONFIG_FOR_SERVER="$TMP_DIR/app.$port.conf"
    write_server_config "$CONFIG_PATH" "$CONFIG_FOR_SERVER" "$port"
    BASE_URL="http://127.0.0.1:$port"
    ./build/oj_server "$CONFIG_FOR_SERVER" >"$TMP_DIR/server.$port.log" 2>&1 &
    SERVER_PID="$!"

    ready=0
    for _ in $(seq 1 30); do
      if curl -sS "$BASE_URL/health" >/dev/null 2>&1; then
        ready=1
        break
      fi
      if ! kill -0 "$SERVER_PID" >/dev/null 2>&1; then
        break
      fi
      sleep 0.1
    done

    if [[ "$ready" -eq 1 ]]; then
      break
    fi

    kill "$SERVER_PID" >/dev/null 2>&1 || true
    wait "$SERVER_PID" >/dev/null 2>&1 || true
    SERVER_PID=""

    if [[ "$port" -eq $((START_PORT + 40)) ]]; then
      cat "$TMP_DIR"/server.*.log >&2
      fail "server exited before becoming ready on ports $START_PORT-$((START_PORT + 40))"
    fi
  done
fi

curl_request() {
  local method="$1"
  local path="$2"
  local body="${3:-}"
  shift 3 || true

  local args=(-sS -X "$method" -D "$HEADERS_FILE" -o "$BODY_FILE" -w "%{http_code}")
  args+=(-H "Content-Type: application/json")
  args+=("$@")
  if [[ -n "$body" ]]; then
    args+=(--data "$body")
  fi
  args+=("$BASE_URL$path")

  HTTP_STATUS="$(curl "${args[@]}")"
  RESPONSE_BODY="$(cat "$BODY_FILE")"
}

assert_response() {
  local label="$1"
  local expected_status="$2"
  local expected_success="$3"
  local expected_message="$4"

  [[ "$HTTP_STATUS" == "$expected_status" ]] ||
    fail "$label status expected $expected_status, got $HTTP_STATUS; body: $RESPONSE_BODY"
  grep -F "\"success\":$expected_success" "$BODY_FILE" >/dev/null ||
    fail "$label success expected $expected_success; body: $RESPONSE_BODY"
  grep -F "\"message\":\"$expected_message\"" "$BODY_FILE" >/dev/null ||
    fail "$label message expected $expected_message; body: $RESPONSE_BODY"
  pass "$label"
}

assert_body_contains() {
  local label="$1"
  local needle="$2"
  grep -F "$needle" "$BODY_FILE" >/dev/null ||
    fail "$label expected body to contain $needle; body: $RESPONSE_BODY"
}

assert_body_not_contains() {
  local label="$1"
  local needle="$2"
  if grep -F "$needle" "$BODY_FILE" >/dev/null; then
    fail "$label expected body not to contain $needle; body: $RESPONSE_BODY"
  fi
}

curl_request GET /health ""
assert_response "GET /health" 200 true ok
assert_body_contains "GET /health" '"status":"ok"'

curl_request POST /api/_echo '{"hello":"world","n":1}'
assert_response "POST /api/_echo" 200 true ok
assert_body_contains "POST /api/_echo" '"hello":"world"'

curl_request POST /api/_echo '{"broken":'
assert_response "POST /api/_echo invalid JSON" 400 false "invalid json"

curl_request GET /api/not-found ""
assert_response "GET /api/not-found" 404 false "not found"

curl_request GET /api/user/me ""
assert_response "GET /api/user/me anonymous" 200 true ok
assert_body_contains "GET /api/user/me anonymous" '"logged_in":false'

curl_request POST /api/submit '{"problem_id":1,"code":"int main(){return 0;}"}'
assert_response "POST /api/submit anonymous" 401 false unauthorized

curl_request POST /api/user/logout '{}'
assert_response "POST /api/user/logout anonymous" 200 true "logged out"

curl_request GET /api/admin/me ""
assert_response "GET /api/admin/me anonymous" 200 true ok
assert_body_contains "GET /api/admin/me anonymous" '"logged_in":false'

curl_request POST /api/admin/logout '{}'
assert_response "POST /api/admin/logout anonymous" 200 true "logged out"

curl_request POST /api/admin/problems '{"title":"Example"}'
assert_response "POST /api/admin/problems anonymous" 401 false unauthorized

curl_request DELETE /api/admin/problems/1 ""
assert_response "DELETE /api/admin/problems/1 anonymous" 401 false unauthorized

if [[ "$MODE" == "basic" ]]; then
  echo "SKIP: DB-backed API checks were not run in --basic mode"
  exit 0
fi

curl_request GET /api/problems ""
assert_response "GET /api/problems" 200 true ok
assert_body_contains "GET /api/problems" '"title":"A+B Problem"'

curl_request GET /api/problems/1 ""
assert_response "GET /api/problems/1" 200 true ok
assert_body_contains "GET /api/problems/1" '"title":"A+B Problem"'
assert_body_contains "GET /api/problems/1" '"samples":'
assert_body_not_contains "GET /api/problems/1 hidden testcase" '10 20'

curl_request GET /api/problems/999999999 ""
assert_response "GET /api/problems/999999999" 404 false "not found"

TEST_USER="api_test_$(date +%s)_$$"
REGISTER_BODY="{\"username\":\"$TEST_USER\",\"password\":\"password\"}"
curl_request POST /api/user/register "$REGISTER_BODY"
assert_response "POST /api/user/register" 201 true registered
assert_body_contains "POST /api/user/register" "\"username\":\"$TEST_USER\""

curl_request POST /api/user/register "$REGISTER_BODY"
assert_response "POST /api/user/register duplicate" 409 false "username exists"

curl_request POST /api/user/register '{"username":"x","password":"123"}'
assert_response "POST /api/user/register invalid" 400 false "invalid username or password"

curl_request POST /api/user/login '{"username":"user1","password":"wrong-password"}'
assert_response "POST /api/user/login wrong password" 401 false "invalid username or password"

curl_request POST /api/user/login '{"username":"user1","password":"password"}' -c "$COOKIE_JAR"
assert_response "POST /api/user/login" 200 true "logged in"
assert_body_contains "POST /api/user/login" '"username":"user1"'
grep -F "oj_user_session" "$COOKIE_JAR" >/dev/null ||
  fail "POST /api/user/login should store oj_user_session cookie"

curl_request GET /api/user/me "" -b "$COOKIE_JAR"
assert_response "GET /api/user/me logged in" 200 true ok
assert_body_contains "GET /api/user/me logged in" '"logged_in":true'

ACCEPTED_BODY='{"problem_id":1,"code":"#include <bits/stdc++.h>\nusing namespace std;\nint main(){int a,b;if(cin>>a>>b){cout<<a+b<<endl;}return 0;}\n"}'
curl_request POST /api/submit "$ACCEPTED_BODY" -b "$COOKIE_JAR"
assert_response "POST /api/submit accepted code" 200 true accepted
assert_body_contains "POST /api/submit accepted code" '"result":"passed"'
assert_body_contains "POST /api/submit accepted code status" '"status":"accepted"'

WRONG_BODY='{"problem_id":1,"code":"#include <bits/stdc++.h>\nusing namespace std;\nint main(){cout<<0<<endl;return 0;}\n"}'
curl_request POST /api/submit "$WRONG_BODY" -b "$COOKIE_JAR"
assert_response "POST /api/submit wrong answer" 200 true wrong_answer
assert_body_contains "POST /api/submit wrong answer" '"result":"failed"'
assert_body_contains "POST /api/submit wrong answer status" '"status":"wrong_answer"'

curl_request POST /api/submit '{"problem_id":1,"code":"int main( {"}' -b "$COOKIE_JAR"
assert_response "POST /api/submit compile error" 200 true compile_error
assert_body_contains "POST /api/submit compile error" '"result":"failed"'
assert_body_contains "POST /api/submit compile error status" '"status":"compile_error"'

curl_request POST /api/submit '{"problem_id":1,"code":""}' -b "$COOKIE_JAR"
assert_response "POST /api/submit empty code" 200 true compile_error
assert_body_contains "POST /api/submit empty code" '"result":"failed"'
assert_body_contains "POST /api/submit empty code status" '"status":"compile_error"'

curl_request POST /api/submit '{"problem_id":999999999,"code":"int main(){return 0;}"}' -b "$COOKIE_JAR"
assert_response "POST /api/submit missing problem" 200 true system_error
assert_body_contains "POST /api/submit missing problem" '"result":"failed"'
assert_body_contains "POST /api/submit missing problem status" '"status":"system_error"'

curl_request POST /api/user/logout '{}' -b "$COOKIE_JAR" -c "$COOKIE_JAR"
assert_response "POST /api/user/logout logged in" 200 true "logged out"

curl_request GET /api/user/me "" -b "$COOKIE_JAR"
assert_response "GET /api/user/me after logout" 200 true ok
assert_body_contains "GET /api/user/me after logout" '"logged_in":false'

curl_request POST /api/admin/login '{"username":"admin","password":"wrong-password"}'
assert_response "POST /api/admin/login wrong password" 401 false "invalid username or password"

curl_request POST /api/admin/login '{"username":"admin","password":"password"}' -c "$ADMIN_COOKIE_JAR"
assert_response "POST /api/admin/login" 200 true "logged in"
assert_body_contains "POST /api/admin/login" '"username":"admin"'
grep -F "oj_admin_session" "$ADMIN_COOKIE_JAR" >/dev/null ||
  fail "POST /api/admin/login should store oj_admin_session cookie"

curl_request GET /api/admin/me "" -b "$ADMIN_COOKIE_JAR"
assert_response "GET /api/admin/me logged in" 200 true ok
assert_body_contains "GET /api/admin/me logged in" '"logged_in":true'

curl_request POST /api/admin/problems '{"title":"Missing fields"}' -b "$ADMIN_COOKIE_JAR"
assert_response "POST /api/admin/problems invalid" 400 false "invalid problem"

ADMIN_TITLE="Curl Admin Problem $TEST_USER"
ADMIN_CREATE_BODY="{\"title\":\"$ADMIN_TITLE\",\"difficulty\":\"easy\",\"description\":\"Read two integers and output their sum.\",\"input_format\":\"Two integers.\",\"output_format\":\"One integer.\",\"sample_input\":\"2 3\\n\",\"sample_output\":\"5\\n\",\"time_limit_ms\":1000,\"memory_limit_kb\":131072,\"compare_mode\":\"strict\",\"samples\":[{\"input\":\"2 3\\n\",\"expected_output\":\"5\\n\"}],\"hidden_testcases\":[{\"input\":\"7 8\\n\",\"expected_output\":\"15\\n\"},{\"input\":\"-2 5\\n\",\"expected_output\":\"3\\n\"}]}"
curl_request POST /api/admin/problems "$ADMIN_CREATE_BODY" -b "$ADMIN_COOKIE_JAR"
assert_response "POST /api/admin/problems" 201 true created
assert_body_contains "POST /api/admin/problems" "\"title\":\"$ADMIN_TITLE\""
CREATED_PROBLEM_ID="$(sed -n 's/.*"id":\([0-9][0-9]*\).*/\1/p' "$BODY_FILE" | head -n 1)"
[[ -n "$CREATED_PROBLEM_ID" ]] ||
  fail "POST /api/admin/problems should return created problem id; body: $RESPONSE_BODY"

curl_request GET /api/problems ""
assert_response "GET /api/problems after admin create" 200 true ok
assert_body_contains "GET /api/problems after admin create" "\"title\":\"$ADMIN_TITLE\""

curl_request GET "/api/problems/$CREATED_PROBLEM_ID" ""
assert_response "GET /api/problems/{created}" 200 true ok
assert_body_contains "GET /api/problems/{created}" "\"title\":\"$ADMIN_TITLE\""
assert_body_contains "GET /api/problems/{created}" '"input":"2 3\n"'
assert_body_not_contains "GET /api/problems/{created} hidden testcase" '7 8'

curl_request DELETE "/api/admin/problems/$CREATED_PROBLEM_ID" "" -b "$ADMIN_COOKIE_JAR"
assert_response "DELETE /api/admin/problems/{created}" 200 true deleted
assert_body_contains "DELETE /api/admin/problems/{created}" '"deleted":true'

curl_request GET "/api/problems/$CREATED_PROBLEM_ID" ""
assert_response "GET /api/problems/{created} after delete" 404 false "not found"

curl_request GET /api/problems ""
assert_response "GET /api/problems after admin delete" 200 true ok
assert_body_not_contains "GET /api/problems after admin delete" "\"title\":\"$ADMIN_TITLE\""

curl_request DELETE "/api/admin/problems/$CREATED_PROBLEM_ID" "" -b "$ADMIN_COOKIE_JAR"
assert_response "DELETE /api/admin/problems/{created} again" 404 false "not found"

curl_request POST /api/admin/logout '{}' -b "$ADMIN_COOKIE_JAR" -c "$ADMIN_COOKIE_JAR"
assert_response "POST /api/admin/logout logged in" 200 true "logged out"

curl_request GET /api/admin/me "" -b "$ADMIN_COOKIE_JAR"
assert_response "GET /api/admin/me after logout" 200 true ok
assert_body_contains "GET /api/admin/me after logout" '"logged_in":false'

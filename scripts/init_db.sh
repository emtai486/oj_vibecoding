#!/usr/bin/env bash
set -euo pipefail

ROOT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
cd "$ROOT_DIR"

CONFIG_PATH="${1:-config/app.conf}"

fail() {
  echo "FAIL: $*" >&2
  exit 1
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

write_client_option() {
  local key="$1"
  local value="$2"

  case "$value" in
    *$'\n'*|*$'\r'*)
      fail "mysql config value for $key must not contain newlines"
      ;;
  esac

  value="${value//\\/\\\\}"
  value="${value//\"/\\\"}"
  printf '%s="%s"\n' "$key" "$value" >>"$CLIENT_CONFIG"
}

command -v mysql >/dev/null || fail "mysql client not found"
[[ -f "$CONFIG_PATH" ]] || fail "config not found: $CONFIG_PATH"
[[ -f sql/schema.sql ]] || fail "sql/schema.sql not found"
[[ -f sql/seed.sql ]] || fail "sql/seed.sql not found"

MYSQL_HOST="$(config_value mysql.host 127.0.0.1)"
MYSQL_PORT="$(config_value mysql.port 3306)"
MYSQL_USER="$(config_value mysql.user oj_user)"
MYSQL_PASSWORD="$(config_value mysql.password '')"
MYSQL_DATABASE="$(config_value mysql.database oj)"
MYSQL_CHARSET="$(config_value mysql.charset utf8mb4)"

if [[ ! "$MYSQL_DATABASE" =~ ^[A-Za-z0-9_]+$ ]]; then
  fail "mysql.database must contain only letters, digits, and underscore"
fi

TMP_DIR="$(mktemp -d)"
trap 'rm -rf "$TMP_DIR"' EXIT

CLIENT_CONFIG="$TMP_DIR/mysql-client.cnf"
chmod 700 "$TMP_DIR"
printf '[client]\n' >"$CLIENT_CONFIG"
write_client_option host "$MYSQL_HOST"
write_client_option port "$MYSQL_PORT"
write_client_option user "$MYSQL_USER"
write_client_option password "$MYSQL_PASSWORD"
write_client_option default-character-set "$MYSQL_CHARSET"
chmod 600 "$CLIENT_CONFIG"

if mysql --defaults-extra-file="$CLIENT_CONFIG" \
    --execute "CREATE DATABASE IF NOT EXISTS \`$MYSQL_DATABASE\` CHARACTER SET utf8mb4 COLLATE utf8mb4_unicode_ci" \
    >/dev/null 2>&1; then
  echo "PASS: ensured database $MYSQL_DATABASE exists"
else
  echo "WARN: could not create database $MYSQL_DATABASE; assuming it already exists and user has access" >&2
fi

mysql --defaults-extra-file="$CLIENT_CONFIG" "$MYSQL_DATABASE" < sql/schema.sql
echo "PASS: imported sql/schema.sql"

mysql --defaults-extra-file="$CLIENT_CONFIG" "$MYSQL_DATABASE" < sql/seed.sql
echo "PASS: imported sql/seed.sql"

#!/bin/bash
# Tests for scripts/odbc-dsn against a scratch odbc.ini (never the real one).
# The scratch file has a hard link, like Excel's container copy, and every
# write must keep it.
#
#   bash tests/test-odbc-dsn.sh
set -uo pipefail

ROOT=$(cd "$(dirname "$0")/.." && pwd)
TOOL="$ROOT/scripts/odbc-dsn"
WORK=$(mktemp -d)
trap 'rm -rf "$WORK"' EXIT

export ODBC_DSN_INI="$WORK/odbc.ini"
LINK="$WORK/container-odbc.ini"
PASS=0 FAIL=0

ok()   { PASS=$((PASS + 1)); echo "  ok   $1"; }
fail() { FAIL=$((FAIL + 1)); echo "  FAIL $1"; [[ -n ${2:-} ]] && printf '%s\n' "$2" | sed 's/^/       /'; }

# assert <label> <expected exit> <command...>; output kept in $OUT
assert() {
  local label=$1 want=$2; shift 2
  OUT=$("$@" 2>&1); local got=$?
  [[ $got -eq $want ]] && ok "$label (exit $got)" || fail "$label: exit $got, want $want" "$OUT"
}
contains()     { [[ $OUT == *"$2"* ]] && ok "$1" || fail "$1: missing '$2'" "$OUT"; }
not_contains() { [[ $OUT != *"$2"* ]] && ok "$1" || fail "$1: unexpected '$2'" "$OUT"; }
ini_has()      { grep -qxF -- "$2" "$ODBC_DSN_INI" && ok "$1" || fail "$1: '$2' not in ini" "$(cat "$ODBC_DSN_INI")"; }
ini_lacks()    { ! grep -qF -- "$2" "$ODBC_DSN_INI" && ok "$1" || fail "$1: '$2' still in ini" "$(cat "$ODBC_DSN_INI")"; }
linked()       { [[ $(stat -f %i "$ODBC_DSN_INI") == $(stat -f %i "$LINK") ]] && ok "$1: hard link kept" || fail "$1: hard link broken"; }

reset_ini() {
  rm -f "$WORK"/odbc.ini* "$LINK"
  cat > "$ODBC_DSN_INI" <<'EOF'
[ODBC Data Sources]
existing = psql

[existing]
Driver      = /Library/ODBC/psqlodbc.bundle/Contents/MacOS/psqlodbcw.so
Description = keep me
Servername  = db.example.com
Port        = 5432
Database    = sales
Username    = alice
Password    = s3cret=with=equals
ReadOnly    = Yes
AnsiCharset = MACINTOSH
EOF
  ln "$ODBC_DSN_INI" "$LINK"
}

echo "== list"
reset_ini
assert "list" 0 "$TOOL" list
contains "list shows DSN" "existing"
contains "list shows server" "db.example.com:5432"
not_contains "list hides password" "s3cret"

echo "== show"
assert "show" 0 "$TOOL" show existing
contains "show has database" "sales"
contains "show masks password" "********"
not_contains "show hides password" "s3cret"
assert "show unknown DSN" 1 "$TOOL" show nope
contains "show unknown message" "DSN 'nope' does not exist"

echo "== add"
reset_ini
before=$(sed -n '/^\[existing\]/,$p' "$ODBC_DSN_INI")
ODBC_DSN_PASSWORD='p@ss w0rd\n=x' assert "add" 0 "$TOOL" add other --host 10.0.0.9 --database erp --user bob
ini_has "add writes section" "[other]"
ini_has "add registers data source" "other = psql"
ini_has "add sets host" "Servername = 10.0.0.9"
ini_has "add defaults port" "Port = 5432"
ini_has "add defaults charset" "AnsiCharset = MACINTOSH"
ini_has "add defaults sslmode" "SSLmode = disable"
ini_has "add keeps password verbatim" 'Password = p@ss w0rd\n=x'
ini_has "add sets driver" "Driver = /Library/ODBC/psqlodbc.bundle/Contents/MacOS/psqlodbcw.so"
[[ $(sed -n '/^\[existing\]/,/^$/p' "$ODBC_DSN_INI") == "$(printf '%s\n' "$before" | sed -n '/^\[existing\]/,/^$/p')" ]] \
  && ok "add leaves other DSN untouched" || fail "add changed [existing]" "$(cat "$ODBC_DSN_INI")"
linked "add"
[[ $(stat -f %Lp "$ODBC_DSN_INI") == 600 ]] && ok "add makes ini private (600)" || fail "ini mode is $(stat -f %Lp "$ODBC_DSN_INI")"
ls "$WORK"/odbc.ini.bak-* >/dev/null 2>&1 && ok "add made a backup" || fail "no backup"

echo "== add validation"
ODBC_DSN_PASSWORD=x assert "add duplicate" 1 "$TOOL" add existing --host h --database d --user u
contains "duplicate message" "DSN 'existing' already exists"
ODBC_DSN_PASSWORD=x assert "add bad name" 1 "$TOOL" add "bad name" --host h --database d --user u
contains "bad name message" "Invalid DSN name"
ODBC_DSN_PASSWORD=x assert "add bad port" 1 "$TOOL" add p1 --host h --port 99999 --database d --user u
contains "bad port message" "Invalid port"
ODBC_DSN_PASSWORD=x assert "add missing host" 1 "$TOOL" add p2 --database d --user u
contains "missing host message" "--host is required"
assert "add unknown option" 1 "$TOOL" add p3 --hots h
contains "unknown option message" "Unknown option: --hots"

echo "== edit"
reset_ini
assert "edit" 0 "$TOOL" edit existing --host new.example.com --port 6543
ini_has "edit replaces host" "Servername = new.example.com"
ini_has "edit replaces port" "Port = 6543"
ini_lacks "edit drops old host" "db.example.com"
ini_has "edit keeps unrelated keys" "Description = keep me"
ini_has "edit keeps password" "Password    = s3cret=with=equals"
linked "edit"
assert "edit unknown DSN" 1 "$TOOL" edit nope --host h

echo "== rename"
reset_ini
assert "rename" 0 "$TOOL" rename existing renamed
ini_has "rename header" "[renamed]"
ini_has "rename data source" "renamed = psql"
ini_lacks "rename removes old header" "[existing]"
ini_has "rename keeps keys" "Password    = s3cret=with=equals"
linked "rename"
ODBC_DSN_PASSWORD=x "$TOOL" add second --host h --database d --user u >/dev/null 2>&1
assert "rename onto existing" 1 "$TOOL" rename renamed second
contains "rename collision message" "DSN 'second' already exists"

echo "== rm"
reset_ini
ODBC_DSN_PASSWORD=x "$TOOL" add other --host h --database d --user u >/dev/null 2>&1
assert "rm" 0 "$TOOL" rm existing --yes
ini_lacks "rm removes section" "[existing]"
ini_lacks "rm removes data source" "existing = psql"
ini_lacks "rm removes keys" "s3cret"
ini_has "rm keeps other DSN" "[other]"
linked "rm"
assert "rm unknown DSN" 1 "$TOOL" rm nope --yes

echo "== test (connection)"
reset_ini
ODBC_DSN_PASSWORD=x "$TOOL" add closed --host 127.0.0.1 --port 1 --database d --user u >/dev/null 2>&1
assert "test unreachable server" 1 "$TOOL" test closed
contains "test reports failure" "Connection failed"

echo "== interactive menu"
reset_ini
OUT=$(printf 'q' | "$TOOL" 2>&1); [[ $? -eq 0 ]] && ok "menu quits with q" || fail "menu q exit" "$OUT"
contains "menu shows title" "ODBC DSN manager"
contains "menu lists DSN" "existing"
# Enter on "Add DSN", answer the prompts (port and sslmode take defaults), then quit.
OUT=$(printf '\nviamenu\nmenu.example.com\n\nmenudb\ncarol\nmenupass\n\nq' | "$TOOL" 2>&1)
[[ $? -eq 0 ]] && ok "menu add flow exits 0" || fail "menu add flow exit" "$OUT"
ini_has "menu add writes section" "[viamenu]"
ini_has "menu add sets host" "Servername = menu.example.com"
ini_has "menu add default port" "Port = 5432"
ini_has "menu add password" "Password = menupass"
not_contains "menu never echoes password" "menupass"
linked "menu add"

echo "== navigation footer, more and version"
reset_ini
OUT=$(printf 'q' | "$TOOL" 2>&1)
contains "main footer" "↑↓  |  Enter  |  M More  |  V Version  |  Q Quit"
# Down to "Edit DSN", Enter opens the DSN picker, q goes back, q quits.
OUT=$(printf '\033[B\nqq' | "$TOOL" 2>&1); [[ $? -eq 0 ]] && ok "submenu back then quit" || fail "submenu exit" "$OUT"
contains "submenu title" "Edit which DSN?"
contains "submenu footer" "↑↓  |  Enter  |  M More  |  V Version  |  Q Back"
OUT=$(printf 'vxq' | "$TOOL" 2>&1); [[ $? -eq 0 ]] && ok "version returns to menu" || fail "version exit" "$OUT"
contains "version screen" "odbc-dsn $("$TOOL" --version | awk '{print $2}')"
contains "version shows driver" "psqlodbc 18.00.0005"
OUT=$(printf 'Mxq' | "$TOOL" 2>&1); [[ $? -eq 0 ]] && ok "more returns to menu" || fail "more exit" "$OUT"
contains "more shows commands" "odbc-dsn rename <old> <new>"
assert "--help" 0 "$TOOL" --help
contains "help is complete" "backup is taken first."
not_contains "help stops at the code" "set -euo pipefail"
OUT=$(printf '\033[B\nvxqq' | "$TOOL" 2>&1); [[ $? -eq 0 ]] && ok "version from submenu" || fail "submenu version exit" "$OUT"
contains "version inside submenu" "psqlodbc 18.00.0005"
assert "--version" 0 "$TOOL" --version
[[ $OUT =~ ^odbc-dsn\ [0-9]+\.[0-9]+\.[0-9]+$ ]] && ok "--version format" || fail "--version format" "$OUT"

echo
echo "passed: $PASS  failed: $FAIL"
[[ $FAIL -eq 0 ]]

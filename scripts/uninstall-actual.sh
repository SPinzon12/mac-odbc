#!/bin/bash
# Uninstall the Actual Technologies ODBC drivers and ODBC Manager, keeping
# the iODBC frameworks: Actual's installer is the only source of
# /Library/Frameworks/iODBC.framework on macOS, and Excel loads it by name
# for every ODBC connection, including the psqlodbc bundle.
#
#   bash scripts/uninstall-actual.sh --dry-run   # show the plan, change nothing
#   sudo bash scripts/uninstall-actual.sh        # do it (asks to confirm)
#
# ini files are backed up as *.bak-<timestamp> before they change.
set -euo pipefail

# Test hook: run against a fake filesystem root instead of /.
PREFIX=${ACTUAL_UNINSTALL_PREFIX:-}
ODBC_DIR="$PREFIX/Library/ODBC"
ODBCINST="$ODBC_DIR/odbcinst.ini"
ODBCINI="$ODBC_DIR/odbc.ini"
MANAGER="$PREFIX/Applications/Utilities/ODBC Manager.app"
PSQL_DRIVER=/Library/ODBC/psqlodbc.bundle/Contents/MacOS/psqlodbcw.so
PKGS=(
  com.actualtechnologies.odbcdriver.ataccess.pkg
  com.actualtechnologies.odbcdriver.atopnsrc.pkg
  com.actualtechnologies.odbcdriver.atoradb.pkg
  com.actualtechnologies.odbcdriver.atsqlsrv.pkg
  com.actualtechnologies.odbcmanager.pkg
)
KEEP=("$PREFIX/Library/Frameworks/iODBC.framework" "$PREFIX/Library/Frameworks/iODBCinst.framework")

DRY=0
case ${1:-} in
  --dry-run|-n) DRY=1 ;;
  '') ;;
  *) echo "Usage: $0 [--dry-run]" >&2; exit 1 ;;
esac
if (( ! DRY )) && [[ -z $PREFIX && $EUID -ne 0 ]]; then
  echo "Needs root: sudo bash $0   (or --dry-run to preview)" >&2
  exit 1
fi

# odbcinst.ini without the Actual drivers, and with [psql] on the bundle.
clean_odbcinst() {
  DRV=$PSQL_DRIVER awk '
    /^\[/ { sec = $0; skip = (sec ~ /^\[Actual /) }
    skip { next }
    sec == "[ODBC Drivers]" && /^Actual / { next }
    sec == "[psql]" && /^[ \t]*Driver[ \t]*=/ { print "Driver = " ENVIRON["DRV"]; next }
    { print }' "$1" | cat -s
}

# odbc.ini without DSNs whose driver is an Actual bundle.
clean_odbcini() {
  awk '
    FNR == NR {
      if ($0 ~ /^\[/) { sec = substr($0, 2, length($0) - 2) }
      else if ($0 ~ /^[ \t]*Driver[ \t]*=/ && $0 ~ /\/Library\/ODBC\/Actual /) actual[sec] = 1
      next
    }
    /^\[/ { sec = substr($0, 2, length($0) - 2); skip = (sec in actual) }
    skip { next }
    sec == "ODBC Data Sources" && index($0, "=") {
      k = substr($0, 1, index($0, "=") - 1); gsub(/^[ \t]+|[ \t]+$/, "", k)
      if (k in actual) next
    }
    { print }' "$1" "$1" | cat -s
}

TS=$(date +%Y%m%d-%H%M%S)
rewrite() {   # rewrite <file> <cleaner>
  local file=$1 cleaner=$2 new
  [[ -f $file ]] || return 0
  new=$($cleaner "$file")
  if [[ $new == "$(cat "$file")" ]]; then
    echo "  unchanged  $file"
  elif (( DRY )); then
    echo "  would edit $file:"
    # diff exits 1 when the files differ, which is the expected case here
    diff <(cat "$file") <(printf '%s\n' "$new") | sed -n 's/^\([<>]\)/      \1/p' || true
  else
    cp -p "$file" "$file.bak-$TS"
    printf '%s\n' "$new" > "$file"
    echo "  edited     $file (backup $file.bak-$TS)"
  fi
}

remove() {
  local path=$1
  [[ -e $path ]] || return 0
  if (( DRY )); then echo "  would remove $path"; else rm -rf "$path"; echo "  removed    $path"; fi
}

run() {
  local b bundles=() p k
  for b in "$ODBC_DIR"/Actual\ *.bundle; do [[ -e $b ]] && bundles+=("$b"); done

  echo "Drivers and apps:"
  for b in ${bundles[@]+"${bundles[@]}"}; do remove "$b"; done
  remove "$MANAGER"
  (( ${#bundles[@]} )) || [[ -e $MANAGER ]] || (( ! DRY )) || echo "  nothing left to remove"
  echo
  echo "Configuration:"
  rewrite "$ODBCINST" clean_odbcinst
  rewrite "$ODBCINI" clean_odbcini
  echo
  echo "Installer receipts:"
  for p in "${PKGS[@]}"; do
    if [[ -n $PREFIX ]] || ! pkgutil --pkg-info "$p" >/dev/null 2>&1; then continue; fi
    if (( DRY )); then echo "  would forget $p"; else pkgutil --forget "$p" >/dev/null; echo "  forgot     $p"; fi
  done
  echo
  echo "Kept (Excel needs them):"
  for k in "${KEEP[@]}"; do [[ -e $k ]] && echo "  $k"; done
}

if (( DRY )); then
  echo "Actual ODBC uninstall (dry run, nothing changes)"; echo
  run
  exit 0
fi

# Show the full plan, confirm, then apply it.
if [[ -z $PREFIX ]]; then
  DRY=1; echo "Actual ODBC uninstall plan"; echo; run; DRY=0
  echo
  printf 'Apply this plan? Type yes: '
  read -r answer
  [[ $answer == yes ]] || { echo "Cancelled, nothing changed."; exit 1; }
  echo
fi
run
echo
echo "Actual's keychain items (DSNs with UseKeychain) are not touched; delete"
echo "them in Keychain Access if you no longer need them."

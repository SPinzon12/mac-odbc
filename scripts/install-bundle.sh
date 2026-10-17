#!/bin/bash
# Build a self-contained psqlodbc bundle under /Library/ODBC from the driver
# compiled against libiodbc (build/src/.libs/psqlodbcw.so), so it speaks
# iODBC's 4-byte SQLWCHAR natively -- the same ABI the Actual drivers use.
# The previous bundle is kept as a timestamped backup next to it.
# Run with: sudo bash scripts/install-bundle.sh   (after scripts/build-driver.sh)
set -euo pipefail

[[ $EUID -eq 0 ]] || { echo "Needs root: sudo bash $0" >&2; exit 1; }

ROOT=$(cd "$(dirname "$0")/.." && pwd)
SRC_DRIVER="$ROOT/build/src/.libs/psqlodbcw.so"
B=/Library/ODBC/psqlodbc.bundle
MACOS="$B/Contents/MacOS"
FW="$B/Contents/Frameworks"
INI=/Library/ODBC/odbc.ini
DRIVER="$MACOS/psqlodbcw.so"
TS=$(date +%Y%m%d-%H%M%S)

[[ -f "$SRC_DRIVER" ]] || { echo "missing driver: $SRC_DRIVER (build it first)" >&2; exit 1; }
if otool -L "$SRC_DRIVER" | grep -q unixodbc; then
  echo "refusing: $SRC_DRIVER links unixODBC, expected a libiodbc build" >&2
  exit 1
fi

if [[ -e "$B" ]]; then
  mv "$B" "$B.bak-$TS"
  echo "Previous bundle saved as $B.bak-$TS"
fi
mkdir -p "$MACOS" "$FW"
cp -p "$SRC_DRIVER" "$DRIVER"

# Copy the full non-system dependency closure into Contents/Frameworks.
# References are resolved by basename, because libpq may point at a
# different prefix (e.g. /usr/local/lib) than where the file really lives.
external_deps() {
  otool -L "$1" | tail -n +2 | awk '{print $1}' \
    | grep -vE '^(/usr/lib/|/System/|@)' || true
}

QUEUE=("$DRIVER")
while ((${#QUEUE[@]})); do
  bin="${QUEUE[0]}"; QUEUE=("${QUEUE[@]:1}")
  while read -r dep; do
    [[ -n "$dep" ]] || continue
    base=$(basename "$dep")
    [[ "$bin" == */"$base" ]] && continue          # the dylib's own id
    [[ -e "$FW/$base" ]] && continue               # already copied
    [[ -e "$dep" ]] || { echo "missing dep: $dep (needed by $bin)" >&2; exit 1; }
    cp "$(readlink -f "$dep")" "$FW/$base"
    chmod u+w "$FW/$base"
    QUEUE+=("$FW/$base")
  done < <(external_deps "$bin")
done

# Rewrite every external dep reference to a sibling inside the bundle.
# $1 = binary, $2 = how @loader_path should reach Contents/Frameworks from it.
retarget() {
  local bin="$1" prefix="$2" base
  external_deps "$bin" | while read -r dep; do
    [[ -n "$dep" ]] || continue
    base=$(basename "$dep")
    [[ "$bin" == */"$base" ]] && continue
    [[ -e "$FW/$base" ]] || continue
    install_name_tool -change "$dep" "@loader_path${prefix}${base}" "$bin" 2>/dev/null
  done
}

retarget "$DRIVER" "/../Frameworks/"
for f in "$FW"/*.dylib; do
  install_name_tool -id "@loader_path/$(basename "$f")" "$f" 2>/dev/null
  retarget "$f" "/"
done

# install_name_tool invalidates the signature; re-sign ad hoc so dyld accepts it.
for f in "$FW"/*.dylib "$DRIVER"; do
  codesign -f -s - "$f" 2>/dev/null
done

chmod -R a+rX "$B"

# Any reference left outside the bundle would fail inside Excel's sandbox.
LEAKS=$(for f in "$DRIVER" "$FW"/*.dylib; do external_deps "$f"; done | sort -u)
if [[ -n "$LEAKS" ]]; then
  echo "error: references outside the bundle remain:" >&2
  echo "$LEAKS" >&2
  exit 1
fi

# Point the existing PSQL DSN at the bundled driver, preserving every other key.
if [[ -f "$INI" ]]; then
  cp -p "$INI" "$INI.bak-$TS"
  awk -v drv="$DRIVER" '
    /^\[PSQL\]/            { inpsql=1; print; next }
    /^\[/                  { inpsql=0 }
    inpsql && /^Driver[ ]*=/ { print "Driver     = " drv; next }
    { print }
  ' "$INI" > "$INI.new"
  mv "$INI.new" "$INI"
  chmod 644 "$INI"
fi

echo "Bundle: $B"
echo "Driver: $DRIVER"
echo
echo "Linked deps:"
otool -L "$DRIVER" | tail -n +2 | sed 's/^/  /'
echo
echo "Bundled frameworks:"
ls "$FW" | sed 's/^/  /'
echo
echo "DSN [PSQL] now points at the bundled driver."
if [[ -e "$B.bak-$TS" ]]; then
  echo "Rollback: sudo rm -rf $B && sudo mv $B.bak-$TS $B && sudo cp -p $INI.bak-$TS $INI"
fi

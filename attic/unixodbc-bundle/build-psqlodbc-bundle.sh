#!/bin/bash
# Build a self-contained psqlodbc bundle under /Library/ODBC, in the same
# layout Excel's sandboxed ODBC path already loads for the Actual drivers.
# Run with: sudo bash build-psqlodbc-bundle.sh
set -euo pipefail

[[ $EUID -eq 0 ]] || { echo "Needs root: sudo bash $0" >&2; exit 1; }

BREW=/opt/homebrew
B=/Library/ODBC/psqlodbc.bundle
MACOS="$B/Contents/MacOS"
FW="$B/Contents/Frameworks"
INI=/Library/ODBC/odbc.ini
DRIVER="$MACOS/psqlodbcw.so"

rm -rf "$B"
mkdir -p "$MACOS" "$FW"

cp -p "$BREW/lib/psqlodbcw.so" "$MACOS/psqlodbcw.so"

# Full runtime closure of psqlodbcw.so (libpq pulls ssl/crypto/krb5; libodbcinst pulls ltdl)
DEPS=(
  "$BREW/opt/libpq/lib/libpq.5.dylib"
  "$BREW/opt/openssl@3/lib/libssl.3.dylib"
  "$BREW/opt/openssl@3/lib/libcrypto.3.dylib"
  "$BREW/opt/unixodbc/lib/libodbcinst.2.dylib"
  "$BREW/opt/libtool/lib/libltdl.7.dylib"
  "$BREW/opt/krb5/lib/libgssapi_krb5.2.2.dylib"
  "$BREW/opt/krb5/lib/libkrb5.3.3.dylib"
  "$BREW/opt/krb5/lib/libk5crypto.3.1.dylib"
  "$BREW/opt/krb5/lib/libcom_err.3.0.dylib"
  "$BREW/opt/krb5/lib/libkrb5support.1.1.dylib"
)

for d in "${DEPS[@]}"; do
  [[ -e "$d" ]] || { echo "missing dep: $d" >&2; exit 1; }
  real=$(readlink -f "$d")
  cp -p "$real" "$FW/$(basename "$real")"
done

# Rewrite every external dep reference to a sibling inside the bundle.
# $1 = binary, $2 = how @loader_path should reach Contents/Frameworks from it.
retarget() {
  local bin="$1" prefix="$2" base dir
  dir=$(dirname "$bin")
  otool -L "$bin" | tail -n +2 | awk '{print $1}' | while read -r dep; do
    [[ -n "$dep" ]] || continue
    base=$(basename "$dep")
    # skip the dylib's own id
    [[ "$dep" == "$dir/$base" || "$dep" == "$FW/$base" || "$dep" == "$MACOS/$base" ]] && continue
    [[ -e "$FW/$base" ]] || continue
    install_name_tool -change "$dep" "@loader_path${prefix}${base}" "$bin"
  done
}

retarget "$DRIVER" "/../Frameworks/"
for f in "$FW"/*.dylib; do
  retarget "$f" "/"
  install_name_tool -id "@loader_path/$(basename "$f")" "$f"
done

# install_name_tool strips the ad-hoc signature; put it back so dyld is happy.
codesign -f -s - "$MACOS/psqlodbcw.so" >/dev/null 2>&1 || true
for f in "$FW"/*.dylib; do
  codesign -f -s - "$f" >/dev/null 2>&1 || true
done

chmod -R a+rX "$B"

# Point the existing PSQL DSN at the bundled driver, preserving every other key.
if [[ -f "$INI" ]]; then
  cp -p "$INI" "$INI.bak-$(date +%Y%m%d-%H%M%S)"
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
echo "DSN [PSQL] now points at the bundled driver."

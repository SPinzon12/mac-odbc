#!/bin/bash
# Build the psqlodbc Unicode driver against libiodbc (4-byte SQLWCHAR, the ABI
# Excel's iODBC uses), with patches/psqlodbc-ansi-charset.patch applied.
#
#   bash scripts/build-driver.sh          -> build/src/.libs/psqlodbcw.so
#   BUILD_DIR=/tmp/x bash scripts/build-driver.sh
#
# Then install it with: sudo bash scripts/install-bundle.sh
set -euo pipefail

VERSION=18_00_0005
URL="https://github.com/postgresql-interfaces/psqlodbc/archive/refs/tags/REL-$VERSION.tar.gz"
SHA256=7c42d4cc5e578698d18d1655561f435dce95864bddb2e0c0e13dae3650b38698

ROOT=$(cd "$(dirname "$0")/.." && pwd)
BUILD_DIR=${BUILD_DIR:-$ROOT/build}
TARBALL="$BUILD_DIR/psqlodbc-REL-$VERSION.tar.gz"
SRC="$BUILD_DIR/src"

for f in libiodbc libpq autoconf automake libtool; do
  brew list --versions "$f" >/dev/null || { echo "missing: brew install $f" >&2; exit 1; }
done
IODBC=$(brew --prefix libiodbc)
LIBPQ=$(brew --prefix libpq)

mkdir -p "$BUILD_DIR"
if [[ ! -f "$TARBALL" ]]; then
  echo "Downloading psqlodbc REL-$VERSION"
  curl -fsSL -o "$TARBALL" "$URL"
fi
echo "$SHA256  $TARBALL" | shasum -a 256 -c - >/dev/null \
  || { echo "checksum mismatch: $TARBALL" >&2; exit 1; }

rm -rf "$SRC" "$BUILD_DIR/psqlodbc-REL-$VERSION"
tar -xzf "$TARBALL" -C "$BUILD_DIR"
mv "$BUILD_DIR/psqlodbc-REL-$VERSION" "$SRC"

cd "$SRC"
patch -p1 --quiet < "$ROOT/patches/psqlodbc-ansi-charset.patch"

echo "Configuring (log: $BUILD_DIR/configure.out)"
autoreconf -i > "$BUILD_DIR/autoreconf.out" 2>&1
PATH="$LIBPQ/bin:$PATH" ./configure --with-iodbc="$IODBC" --with-libpq="$LIBPQ" \
  --enable-pthreads > "$BUILD_DIR/configure.out" 2>&1

# configure links libiodbc too, but the driver only needs libiodbcinst:
# dropping it keeps a second iODBC copy out of Excel. iconv backs AnsiCharset.
echo "Building (log: $BUILD_DIR/make.out)"
make -j"$(sysctl -n hw.ncpu)" psqlodbcw.la \
  LDFLAGS="-L$IODBC/lib -liodbcinst -L$LIBPQ/lib -Wl,-dead_strip_dylibs" \
  LIBS="-lpq -lpthread -liconv" > "$BUILD_DIR/make.out" 2>&1

DRIVER="$SRC/.libs/psqlodbcw.so"
echo
echo "Driver: $DRIVER"
otool -L "$DRIVER" | tail -n +2 | sed 's/^/  /'

#!/usr/bin/env bash
# ═══════════════════════════════════════════════════════════════════
#  QMark — Build Static Dependencies
#  Builds SQLite3 and libsodium as static libraries for a target.
#
#  Usage:
#    ./build-deps.sh linux-x64     # Native Linux x64
#    ./build-deps.sh linux-x86     # Linux x86 (needs gcc-multilib)
#    ./build-deps.sh win-x64       # Windows x64 cross-compile
#    ./build-deps.sh win-x86       # Windows x86 cross-compile
#
#  Dependencies must be downloaded first:
#    - SQLite3 amalgamation: https://www.sqlite.org/download.html
#    - libsodium:           https://github.com/jedisct1/libsodium/releases
#
#  Set environment variables:
#    SQLITE3_SRC  — Path to sqlite3.c (default: downloads automatically)
#    SODIUM_SRC   — Path to libsodium source dir (default: downloads automatically)
#    DEPS_DIR     — Output directory for built libraries
# ═══════════════════════════════════════════════════════════════════
set -euo pipefail

SCRIPT_DIR="$(cd "$(dirname "$0")" && pwd)"
TARGET="${1:-}"
DEPS_DIR="${DEPS_DIR:-$SCRIPT_DIR/deps}"

if [ -z "$TARGET" ]; then
    echo "Usage: $0 <target>"
    echo "  Targets: linux-x64, linux-x86, win-x64, win-x86"
    exit 1
fi

echo "═══════════════════════════════════════════════════════════════"
echo "  QMark — Building Dependencies for $TARGET"
echo "═══════════════════════════════════════════════════════════════"

mkdir -p "$DEPS_DIR/$TARGET"

# ── Target configuration ──────────────────────────────────────────
case "$TARGET" in
    linux-x64)
        CC="gcc"
        AR="ar"
        RANLIB="ranlib"
        CFLAGS=""
        CONFIGURE_HOST=""
        ;;
    linux-x86)
        CC="gcc -m32"
        AR="ar"
        RANLIB="ranlib"
        CFLAGS="-m32"
        CONFIGURE_HOST=""
        ;;
    win-x64)
        CC="x86_64-w64-mingw32-gcc"
        AR="x86_64-w64-mingw32-ar"
        RANLIB="x86_64-w64-mingw32-ranlib"
        CFLAGS=""
        CONFIGURE_HOST="--host=x86_64-w64-mingw32"
        ;;
    win-x86)
        CC="i686-w64-mingw32-gcc"
        AR="i686-w64-mingw32-ar"
        RANLIB="i686-w64-mingw32-ranlib"
        CFLAGS="-m32"
        CONFIGURE_HOST="--host=i686-w64-mingw32"
        ;;
    *)
        echo "ERROR: Unknown target '$TARGET'" >&2
        echo "  Valid targets: linux-x64, linux-x86, win-x64, win-x86"
        exit 1
        ;;
esac

OUT_DIR="$DEPS_DIR/$TARGET"

# ── SQLite3 ───────────────────────────────────────────────────────
echo ""
echo "[1/2] SQLite3..."

SQLITE3_SRC="${SQLITE3_SRC:-}"
SQLITE3_URL="https://www.sqlite.org/2024/sqlite-amalgamation-3460100.zip"

if [ -z "$SQLITE3_SRC" ] || [ ! -f "$SQLITE3_SRC/sqlite3.c" ]; then
    # Try to find sqlite3.c in common locations
    for candidate in \
        "$SCRIPT_DIR/sqlitecpp/sqlite3/sqlite3.c" \
        "$SCRIPT_DIR/sqlite3.c" \
        /usr/include/sqlite3.h; do
        if [ -f "$candidate" ]; then
            SQLITE3_SRC="$(dirname "$candidate")"
            break
        fi
    done
fi

if [ -z "$SQLITE3_SRC" ] || [ ! -f "$SQLITE3_SRC/sqlite3.c" ]; then
    echo "  Downloading SQLite3 amalgamation..."
    cd /tmp
    if [ ! -f "sqlite-amalgamation-3460100.zip" ]; then
        wget -q "$SQLITE3_URL" || curl -sLO "$SQLITE3_URL"
    fi
    unzip -qo "sqlite-amalgamation-3460100.zip"
    SQLITE3_SRC="/tmp/sqlite-amalgamation-3460100"
fi

echo "  Compiling sqlite3.c..."
cd "$SQLITE3_SRC"
$CC $CFLAGS -c sqlite3.c -o sqlite3.o -O2 -DSQLITE_THREADSAFE=1 -fPIC
$AR rcs "$OUT_DIR/libsqlite3.a" sqlite3.o
echo "  -> $OUT_DIR/libsqlite3.a"

# Copy header
cp sqlite3.h "$OUT_DIR/"

# ── libsodium ─────────────────────────────────────────────────────
echo ""
echo "[2/2] libsodium..."

SODIUM_SRC="${SODIUM_SRC:-}"
SODIUM_VERSION="1.0.20"
SODIUM_URL="https://github.com/jedisct1/libsodium/releases/download/$SODIUM_VERSION-RELEASE/libsodium-$SODIUM_VERSION.tar.gz"

if [ -z "$SODIUM_SRC" ] || [ ! -f "$SODIUM_SRC/configure" ]; then
    # Try common locations
    for candidate in \
        "$SCRIPT_DIR/libsodium-$SODIUM_VERSION" \
        /tmp/libsodium-$SODIUM_VERSION; do
        if [ -d "$candidate" ]; then
            SODIUM_SRC="$candidate"
            break
        fi
    done
fi

if [ -z "$SODIUM_SRC" ] || [ ! -f "$SODIUM_SRC/configure" ]; then
    echo "  Downloading libsodium $SODIUM_VERSION..."
    cd /tmp
    if [ ! -f "libsodium-$SODIUM_VERSION.tar.gz" ]; then
        wget -q "$SODIUM_URL" || curl -sL "$SODIUM_URL" -o "libsodium-$SODIUM_VERSION.tar.gz"
    fi
    tar xzf "libsodium-$SODIUM_VERSION.tar.gz"
    SODIUM_SRC="/tmp/libsodium-$SODIUM_VERSION"
fi

echo "  Configuring and building libsodium..."
cd "$SODIUM_SRC"
./configure \
    $CONFIGURE_HOST \
    --prefix="$OUT_DIR" \
    --disable-shared \
    --enable-static \
    --quiet
make -j"$(nproc 2>/dev/null || echo 4)" --quiet
make install --quiet
echo "  -> $OUT_DIR/lib/libsodium.a"

# ── Summary ───────────────────────────────────────────────────────
echo ""
echo "═══════════════════════════════════════════════════════════════"
echo "  Dependencies built for $TARGET:"
echo "    SQLite3:   $OUT_DIR/libsqlite3.a"
echo "    libsodium: $OUT_DIR/lib/libsodium.a"
echo "═══════════════════════════════════════════════════════════════"
echo ""
echo "To build QMark, set these environment variables:"
echo "  SQLITE3_DIR=$OUT_DIR"
echo "  SODIUM_DIR=$OUT_DIR"
if [[ "$TARGET" == win-* ]]; then
    echo "  Then run: ./build-$TARGET.sh"
else
    echo "  Then run: ./build-$TARGET.sh"
fi

#!/usr/bin/env bash
# ═══════════════════════════════════════════════════════════════════
#  QMark — Build for Linux i686 (32-bit, cross-compile from x64)
#  Completely statically linked. No external shared libraries needed.
#
#  Prerequisites:
#    sudo apt install gcc-multilib g++-multilib qt6-base-dev
#    # Or for cross-compiled Qt:
#    # Build Qt6 static for i686 with -platform linux-g++-32
# ═══════════════════════════════════════════════════════════════════
set -euo pipefail

SCRIPT_DIR="$(cd "$(dirname "$0")" && pwd)"
SRC_DIR="$SCRIPT_DIR/src"
BUILD_DIR="$SCRIPT_DIR/build-linux-x86"
OUTPUT="$SCRIPT_DIR/QMark-x86"

JOBS="${JOBS:-$(nproc 2>/dev/null || echo 4)}"

echo "═══════════════════════════════════════════════════════════════"
echo "  QMark — Linux x86 (32-bit) Build"
echo "═══════════════════════════════════════════════════════════════"

# ── Check dependencies ────────────────────────────────────────────
echo "[1/4] Checking dependencies..."

check_pkg() {
    if ! command -v "$1" &>/dev/null; then
        echo "ERROR: $1 not found. Install: $2" >&2
        exit 1
    fi
}

check_pkg g++ "g++"
check_pkg pkg-config "pkg-config"

# Check for 32-bit multilib support
if ! echo 'int main(){}' | g++ -m32 -x c++ - -o /dev/null 2>/dev/null; then
    echo "ERROR: 32-bit compilation not supported." >&2
    echo "  Install: sudo apt install gcc-multilib g++-multilib" >&2
    exit 1
fi

# ── Prepare build directory ───────────────────────────────────────
echo "[2/4] Preparing build directory..."
mkdir -p "$BUILD_DIR"
cd "$SRC_DIR"

# ── Build via qmake ──────────────────────────────────────────────
echo "[3/4] Building with qmake (32-bit cross)..."
make clean 2>/dev/null || true

# Try system qmake6 first, fall back to qmake
QMAKE=$(command -v qmake6 2>/dev/null || command -v qmake 2>/dev/null || echo "")
if [ -z "$QMAKE" ]; then
    echo "ERROR: qmake6 or qmake not found. Install Qt6 dev packages." >&2
    exit 1
fi

"$QMAKE" QMark.pro \
    QMAKE_CC=gcc \
    QMAKE_CXX=g++ \
    QMAKE_CFLAGS="-m32 -static" \
    QMAKE_CXXFLAGS="-m32 -static" \
    QMAKE_LFLAGS="-m32 -static -static-libgcc -static-libstdc++" \
    -o "$BUILD_DIR/Makefile"

make -C "$BUILD_DIR" -j"$JOBS"

# ── Copy output ──────────────────────────────────────────────────
echo "[4/4] Copying binary..."
if [ -f "$BUILD_DIR/app/QMark" ]; then
    cp "$BUILD_DIR/app/QMark" "$OUTPUT"
    chmod +x "$OUTPUT"
    echo ""
    echo "═══════════════════════════════════════════════════════════════"
    echo "  Build complete: $OUTPUT"
    echo "  Size: $(ls -lh "$OUTPUT" | awk '{print $5}')"
    echo "  Type: $(file "$OUTPUT")"
    echo "═══════════════════════════════════════════════════════════════"
else
    echo "ERROR: Build failed. Binary not found at $BUILD_DIR/app/QMark" >&2
    exit 1
fi

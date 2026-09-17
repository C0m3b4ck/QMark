#!/usr/bin/env bash
# ═══════════════════════════════════════════════════════════════════
#  QMark — Build for Linux x86_64 (native)
#  Completely statically linked. No external shared libraries needed.
# ═══════════════════════════════════════════════════════════════════
set -euo pipefail

SCRIPT_DIR="$(cd "$(dirname "$0")" && pwd)"
SRC_DIR="$SCRIPT_DIR/src"
BUILD_DIR="$SCRIPT_DIR/build-linux-x64"
OUTPUT="$SCRIPT_DIR/QMark-x64"

CXX="${CXX:-g++}"
JOBS="${JOBS:-$(nproc 2>/dev/null || echo 4)}"

echo "═══════════════════════════════════════════════════════════════"
echo "  QMark — Linux x64 Build"
echo "═══════════════════════════════════════════════════════════════"

# ── Check dependencies ────────────────────────────────────────────
echo "[1/4] Checking dependencies..."

check_pkg() {
    if ! command -v "$1" &>/dev/null; then
        echo "ERROR: $1 not found. Install: $2" >&2
        exit 1
    fi
}

check_pkg qmake6 "qt6-base-dev (Debian/Ubuntu) or qt6-qtbase (Fedora)"
check_pkg g++ "g++"
check_pkg pkg-config "pkg-config"

# Check Qt6 SQL module
if ! pkg-config --exists Qt6Sql 2>/dev/null; then
    echo "WARNING: Qt6Sql not found via pkg-config, build may fail."
    echo "  Install: qt6-base-dev (includes Qt6Sql)"
fi

# ── Prepare build directory ───────────────────────────────────────
echo "[2/4] Preparing build directory..."
mkdir -p "$BUILD_DIR"
cd "$SRC_DIR"

# ── Build via qmake ──────────────────────────────────────────────
echo "[3/4] Building with qmake..."
make clean 2>/dev/null || true

qmake6 QMark.pro \
    QMAKE_CFLAGS="-static" \
    QMAKE_CXXFLAGS="-static" \
    QMAKE_LFLAGS="-static -static-libgcc -static-libstdc++" \
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

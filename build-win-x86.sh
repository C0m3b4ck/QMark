#!/usr/bin/env bash
# ═══════════════════════════════════════════════════════════════════
#  QMark — Build for Windows i686 (32-bit, cross-compile from Linux)
#  Completely statically linked. No DLLs required.
#  Targets Windows 2000 and newer.
#
#  Prerequisites:
#    sudo apt install mingw-w64
#
#  Dependencies (pre-built static libraries):
#    Qt 6     — Built from source with -static for mingw32
#    SQLite3  — Amalgamation compiled with mingw32-gcc
#    libsodium — ./configure --host=i686-w64-mingw32 --disable-shared --enable-static
#    SQLiteCpp — Built from vendored source
#
#  Set environment variables to override default paths:
#    QT_DIR       — Qt6 static install prefix (default: /opt/qt6-win32)
#    SQLITE3_DIR  — SQLite3 amalgamation dir (default: /opt/sqlite3)
#    SODIUM_DIR   — libsodium install prefix (default: /opt/libsodium-win32)
#    SQLITECPP_DIR — SQLiteCpp source dir (default: $SCRIPT_DIR/sqlitecpp)
# ═══════════════════════════════════════════════════════════════════
set -euo pipefail

SCRIPT_DIR="$(cd "$(dirname "$0")" && pwd)"
SRC_DIR="$SCRIPT_DIR/src"
BUILD_DIR="$SCRIPT_DIR/build-win-x86"
OUTPUT="$SCRIPT_DIR/QMark-x86.exe"

# ── Toolchain ─────────────────────────────────────────────────────
CXX=i686-w64-mingw32-g++
CC=i686-w64-mingw32-gcc
AR=i686-w64-mingw32-ar

# ── Dependency paths (override via environment) ───────────────────
find_dir() {
    local desc="$1"; shift
    for d in "$@"; do
        if [ -d "$d" ]; then echo "$d"; return; fi
    done
    echo ""
}

QT_DIR="${QT_DIR:-$(find_dir "Qt6 static" \
    /opt/qt6-win32 \
    "$HOME/qmark-build/qt-static-win32" \
    /home/sb3x/qmark-build/qt-static-win32 \
    /tmp/qt6-win32)}"
SQLITE3_DIR="${SQLITE3_DIR:-$(find_dir "SQLite3" \
    /opt/sqlite3 \
    "$HOME/qmark-build" \
    /home/sb3x/qmark-build \
    /tmp/sqlite3)}"
SODIUM_DIR="${SODIUM_DIR:-$(find_dir "libsodium" \
    /opt/libsodium-win32 \
    "$HOME/qmark-build/sodium-win32" \
    /home/sb3x/qmark-build/sodium-win32 \
    /tmp/libsodium-win32)}"
SQLITECPP_DIR="${SQLITECPP_DIR:-$(find_dir "SQLiteCpp" \
    "$SCRIPT_DIR/sqlitecpp" \
    "$SCRIPT_DIR")}"

JOBS="${JOBS:-$(nproc 2>/dev/null || echo 4)}"

echo "═══════════════════════════════════════════════════════════════"
echo "  QMark — Windows x86 (32-bit) Build"
echo "═══════════════════════════════════════════════════════════════"

# ── Check dependencies ────────────────────────────────────────────
echo "[1/6] Checking dependencies..."

check_tool() {
    if ! command -v "$1" &>/dev/null; then
        echo "ERROR: $1 not found. $2" >&2
        exit 1
    fi
}

check_tool "$CXX" "Install: sudo apt install mingw-w64"
check_tool "$CC" "Install: sudo apt install mingw-w64"

check_dir() {
    if [ ! -d "$1" ]; then
        echo "ERROR: $2 not found at $1" >&2
        echo "  Set ${3} environment variable to the correct path." >&2
        exit 1
    fi
}

check_dir "$QT_DIR/lib" "Qt6 static libraries" "QT_DIR"
check_dir "$SQLITE3_DIR" "SQLite3 amalgamation" "SQLITE3_DIR"
check_dir "$SODIUM_DIR" "libsodium" "SODIUM_DIR"
check_dir "$SQLITECPP_DIR/include" "SQLiteCpp headers" "SQLITECPP_DIR"

if [ ! -f "$QT_DIR/plugins/platforms/libqwindows.a" ]; then
    echo "ERROR: Qt Windows platform plugin not found at $QT_DIR/plugins/platforms/libqwindows.a" >&2
    echo "  Build Qt with -static to generate static plugins." >&2
    exit 1
fi

# Find moc
MOC_PATH="${QT_HOST_DIR:-/opt/qt6-host}/libexec/moc"
if [ ! -x "$MOC_PATH" ]; then
    for candidate in \
        "$HOME/qmark-build/qt-host-tools/libexec/moc" \
        /home/sb3x/qmark-build/qt-host-tools/libexec/moc \
        /opt/qt6-host/libexec/moc \
        "$QT_DIR/../qt-host-tools/libexec/moc"; do
        if [ -x "$candidate" ]; then
            MOC_PATH="$candidate"
            break
        fi
    done
fi
if [ ! -x "$MOC_PATH" ]; then
    echo "ERROR: moc not found. Set QT_HOST_DIR to Qt host tools prefix." >&2
    exit 1
fi

# ── Prepare build directory ───────────────────────────────────────
echo "[2/6] Preparing build directory..."
mkdir -p "$BUILD_DIR/obj"
cd "$SRC_DIR"

# ── Common flags ──────────────────────────────────────────────────
CXXFLAGS="-std=c++17 -O2 -Wall -m32 -DWIN32 -DUNICODE -D_UNICODE -DMINGW_HAS_SECURE_API=1 -DQT_STATICPLUGIN \
  -I$QT_DIR/include \
  -I$QT_DIR/include/QtCore \
  -I$QT_DIR/include/QtGui \
  -I$QT_DIR/include/QtWidgets \
  -I$QT_DIR/include/QtSql \
  -I$SQLITECPP_DIR/include \
  -I$SQLITE3_DIR \
  -I$SODIUM_DIR/include \
  -I$BUILD_DIR/obj"

# ── Build SQLiteCpp static library ────────────────────────────────
echo "[3/6] Building SQLiteCpp static library..."
SQLITECPP_SRCS=(
    $SQLITECPP_DIR/src/Database.cpp
    $SQLITECPP_DIR/src/Statement.cpp
    $SQLITECPP_DIR/src/VirtualDB.cpp
    $SQLITECPP_DIR/src/VirtualTable.cpp
    $SQLITECPP_DIR/src/Column.cpp
    $SQLITECPP_DIR/src/Exception.cpp
    $SQLITECPP_DIR/src/Transaction.cpp
)

SQLITECPP_OBJS=""
for src in "${SQLITECPP_SRCS[@]}"; do
    obj="$BUILD_DIR/obj/$(basename "${src%.cpp}.o")"
    $CXX $CXXFLAGS -c "$src" -o "$obj" 2>&1 | head -3
    SQLITECPP_OBJS="$SQLITECPP_OBJS $obj"
done
$AR rcs "$BUILD_DIR/obj/libSQLiteCpp.a" $SQLITECPP_OBJS
echo "  libSQLiteCpp.a built"

# ── Compile QMark sources ────────────────────────────────────────
echo "[4/6] Compiling QMark sources..."

QMARK_SRCS=(
    businesslogic.cpp
    crypto.cpp
    main.cpp
    mainwindow.cpp
    sanitize_string.cpp
    sqlite_dataaccess.cpp
)

QMARK_OBJS=""
for src in "${QMARK_SRCS[@]}"; do
    obj="$BUILD_DIR/obj/$(basename "${src%.cpp}.o")"
    echo "  $src..."
    $CXX $CXXFLAGS -c "$src" -o "$obj" 2>&1 | head -5
    QMARK_OBJS="$QMARK_OBJS $obj"
done

echo "  moc_mainwindow.cpp..."
"$MOC_PATH" mainwindow.h -o "$BUILD_DIR/obj/moc_mainwindow.cpp" 2>&1
$CXX $CXXFLAGS -c "$BUILD_DIR/obj/moc_mainwindow.cpp" -o "$BUILD_DIR/obj/moc_mainwindow.o" 2>&1 | head -5
QMARK_OBJS="$QMARK_OBJS $BUILD_DIR/obj/moc_mainwindow.o"

# ── Link ──────────────────────────────────────────────────────────
echo "[5/6] Linking QMark.exe..."

rm -f "$BUILD_DIR/QMark.exe"

$CXX -static -static-libgcc -static-libstdc++ -m32 \
  $QMARK_OBJS \
  -Wl,--start-group \
  -L"$BUILD_DIR/obj" -lSQLiteCpp \
  -L"$SQLITE3_DIR" -lsqlite3 \
  -L"$SODIUM_DIR/lib" -lsodium \
  -L"$QT_DIR/lib" \
  -lQt6Widgets -lQt6Gui -lQt6Core -lQt6Sql \
  -lQt6Svg -lQt6SvgWidgets -lQt6EntryPoint \
  -lQt6BundledFreetype -lQt6BundledHarfbuzz -lQt6BundledLibjpeg -lQt6BundledLibpng -lQt6BundledPcre2 -lQt6BundledZLIB \
  -L"$QT_DIR/plugins/platforms" -lqwindows \
  -L"$QT_DIR/plugins/sqldrivers" -lqsqlite \
  -L"$QT_DIR/plugins/imageformats" -lqgif -lqico -lqjpeg -lqsvg -lqtiff -lqwebp -lqtga -lqwbmp -lqicns \
  -L"$QT_DIR/plugins/iconengines" -lqsvgicon \
  -L"$QT_DIR/plugins/styles" -lqmodernwindowsstyle \
  -ld3d11 -ld3d12 -ldxgi -ldwrite -lsetupapi -ld3d9 -lshcore -lwtsapi32 \
  -lauthz -lmincore -lntdll -lnetapi32 -luserenv -ldbghelp \
  -lmingw32 -lwindowscodecs -limm32 -lole32 -loleaut32 -luuid -lws2_32 \
  -ladvapi32 -lshell32 -lgdi32 -lcomdlg32 -lcomctl32 -luser32 -lwinmm \
  -lusp10 -lshlwapi -ldwmapi -luxtheme -lglu32 -lopengl32 -lversion \
  -Wl,--end-group \
  -Wl,--allow-multiple-definition \
  -o "$BUILD_DIR/QMark.exe" \
  -mwindows 2>&1

# ── Verify and copy ──────────────────────────────────────────────
echo "[6/6] Verifying..."
if [ -f "$BUILD_DIR/QMark.exe" ]; then
    cp "$BUILD_DIR/QMark.exe" "$OUTPUT"
    echo ""
    echo "═══════════════════════════════════════════════════════════════"
    echo "  Build complete: $OUTPUT"
    echo "  Size: $(ls -lh "$OUTPUT" | awk '{print $5}')"
    echo "  Type: $(file "$OUTPUT")"
    echo "═══════════════════════════════════════════════════════════════"
else
    echo "ERROR: Link failed." >&2
    exit 1
fi

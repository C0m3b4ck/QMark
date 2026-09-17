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
#    SQLiteCpp — Built from vendored source (auto-built by this script)
#
#  Dependency search order (first match wins):
#    1. Environment variables: QT_DIR, SQLITE3_DIR, SODIUM_DIR
#    2. Project-local: ./deps/qt6-win32, ./deps/sqlite3, ./deps/libsodium-win32
#    3. User home: ~/qt6-win32, ~/sqlite3, ~/libsodium-win32
#    4. System: /opt/qt6-win32, /opt/sqlite3, /opt/libsodium-win32
#
#  To build dependencies first:  ./build-deps.sh win-x86
# ═══════════════════════════════════════════════════════════════════
set -euo pipefail

SCRIPT_DIR="$(cd "$(dirname "$0")" && pwd)"
SRC_DIR="$SCRIPT_DIR/src"
BUILD_DIR="$SCRIPT_DIR/build-win-x86"
OUTPUT="$SCRIPT_DIR/QMark-x86.exe"

# ── Toolchain ─────────────────────────────────────────────────────
CXX="${CXX:-i686-w64-mingw32-g++}"
CC="${CC:-i686-w64-mingw32-gcc}"
AR="${AR:-i686-w64-mingw32-ar}"

# ── Path detection ────────────────────────────────────────────────
find_dir() {
    for d in "$@"; do
        [ -d "$d" ] && echo "$d" && return
    done
}

QT_DIR="${QT_DIR:-$(find_dir \
    "$SCRIPT_DIR/deps/qt6-win32" \
    "$HOME/qt6-win32" \
    "$HOME/deps/qt6-win32" \
    /opt/qt6-win32)}"
SQLITE3_DIR="${SQLITE3_DIR:-$(find_dir \
    "$SCRIPT_DIR/deps/sqlite3" \
    "$HOME/sqlite3" \
    "$HOME/deps/sqlite3" \
    /opt/sqlite3)}"
SODIUM_DIR="${SODIUM_DIR:-$(find_dir \
    "$SCRIPT_DIR/deps/libsodium-win32" \
    "$HOME/libsodium-win32" \
    "$HOME/deps/libsodium-win32" \
    /opt/libsodium-win32)}"
SQLITECPP_DIR="${SQLITECPP_DIR:-$SCRIPT_DIR/sqlitecpp}"

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

missing=0
if [ -z "$QT_DIR" ]; then
    echo "ERROR: Qt6 static libraries not found." >&2
    echo "  Set QT_DIR to your Qt6 static install prefix." >&2
    echo "  Or place files in ./deps/qt6-win32/" >&2
    missing=1
fi
if [ -z "$SQLITE3_DIR" ] || [ ! -f "$SQLITE3_DIR/sqlite3.c" ]; then
    echo "ERROR: SQLite3 amalgamation not found." >&2
    echo "  Set SQLITE3_DIR to the directory containing sqlite3.c." >&2
    echo "  Or place files in ./deps/sqlite3/" >&2
    missing=1
fi
if [ -z "$SODIUM_DIR" ]; then
    echo "ERROR: libsodium not found." >&2
    echo "  Set SODIUM_DIR to your libsodium install prefix." >&2
    echo "  Or place files in ./deps/libsodium-win32/" >&2
    missing=1
fi
if [ ! -d "$SQLITECPP_DIR/include" ]; then
    echo "ERROR: SQLiteCpp not found at $SQLITECPP_DIR" >&2
    missing=1
fi
[ "$missing" -eq 1 ] && exit 1

echo "  QT_DIR:      $QT_DIR"
echo "  SQLITE3_DIR: $SQLITE3_DIR"
echo "  SODIUM_DIR:  $SODIUM_DIR"

if [ ! -f "$QT_DIR/plugins/platforms/libqwindows.a" ]; then
    echo "ERROR: Qt Windows platform plugin not found at $QT_DIR/plugins/platforms/" >&2
    exit 1
fi

find_moc() {
    for d in \
        "${QT_HOST_DIR:-}" \
        "$SCRIPT_DIR/deps/qt6-host/bin" \
        "$SCRIPT_DIR/deps/qt6-host/libexec" \
        "$HOME/qt6-host/libexec" \
        "$HOME/deps/qt6-host/libexec" \
        /opt/qt6-host/libexec \
        "$QT_DIR/../qt6-host/libexec" \
        "$QT_DIR/../qt-host-tools/libexec"; do
        [ -z "$d" ] && continue
        for f in "$d/moc" "$d/qt6/moc"; do
            [ -x "$f" ] && echo "$f" && return
        done
    done
}

MOC_PATH="$(find_moc)"
if [ -z "$MOC_PATH" ]; then
    echo "ERROR: moc not found. Set QT_HOST_DIR to Qt host tools prefix." >&2
    exit 1
fi
echo "  MOC:         $MOC_PATH"

find_uic() {
    for d in \
        "${QT_HOST_DIR:-}" \
        "$SCRIPT_DIR/deps/qt6-host/bin" \
        "$SCRIPT_DIR/deps/qt6-host/libexec" \
        "$HOME/qt6-host/libexec" \
        "$HOME/deps/qt6-host/libexec" \
        /opt/qt6-host/libexec \
        "$QT_DIR/../qt6-host/libexec" \
        "$QT_DIR/../qt-host-tools/libexec"; do
        [ -z "$d" ] && continue
        for f in "$d/uic" "$d/qt6/uic"; do
            [ -x "$f" ] && echo "$f" && return
        done
    done
}

UIC_PATH="$(find_uic)"
if [ -z "$UIC_PATH" ]; then
    echo "ERROR: uic not found. Set QT_HOST_DIR to Qt host tools prefix." >&2
    exit 1
fi
echo "  UIC:         $UIC_PATH"

# ── Prepare build directory ───────────────────────────────────────
echo "[2/7] Preparing build directory..."
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

# ── Generate UI headers ───────────────────────────────────────────
echo "[3/7] Generating UI headers..."
"$UIC_PATH" mainwindow.ui -o "$BUILD_DIR/obj/ui_mainwindow.h" 2>&1
mkdir -p "$SRC_DIR/app"
cp "$BUILD_DIR/obj/ui_mainwindow.h" "$SRC_DIR/app/ui_mainwindow.h"

# ── Build SQLiteCpp static library ────────────────────────────────
echo "[4/7] Building SQLiteCpp static library..."
SQLITECPP_SRCS=$(find "$SQLITECPP_DIR/src" -name '*.cpp' -not -name '*test*' -not -name '*example*' | sort)

if [ -z "$SQLITECPP_SRCS" ]; then
    echo "ERROR: No SQLiteCpp source files found in $SQLITECPP_DIR/src/" >&2
    exit 1
fi

SQLITECPP_OBJS=""
for src in $SQLITECPP_SRCS; do
    obj="$BUILD_DIR/obj/$(basename "${src%.cpp}.o")"
    $CXX $CXXFLAGS -c "$src" -o "$obj" 2>&1 | head -3
    SQLITECPP_OBJS="$SQLITECPP_OBJS $obj"
done
$AR rcs "$BUILD_DIR/obj/libSQLiteCpp.a" $SQLITECPP_OBJS
echo "  libSQLiteCpp.a built"

# ── Compile QMark sources ────────────────────────────────────────
echo "[5/7] Compiling QMark sources..."

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
echo "[6/7] Linking QMark.exe..."

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
echo "[7/7] Verifying..."
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

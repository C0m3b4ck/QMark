# Build Instructions for QMark

> Release binaries (all build-script targets) are **completely statically linked** — no external DLLs or shared libraries required. The native **MSYS2** route links dynamically against MinGW-w64 packages (see [MSYS2](#prerequisites-msys2--native-windows-build)).

---

## Quick Start

```bash
# 1. Install prerequisites
sudo apt install build-essential g++ mingw-w64 qt6-base-dev

# 2. Place dependencies (see Dependency Setup below)
mkdir -p deps
ln -s /path/to/qt6-static    deps/qt6-win64
ln -s /path/to/sqlite3       deps/sqlite3
ln -s /path/to/libsodium     deps/libsodium-win64
ln -s /path/to/qt6-host      deps/qt6-host

# 3. Build
./build-win-x64.sh            # Windows x64
./build-win-x86.sh            # Windows x86
./build-linux-x64.sh          # Linux x64
./build-all.sh                # All targets
```

---

## Build Scripts

| Script | Target | Output |
|---|---|---|
| `build-linux-x64.sh` | Linux x86_64 (native) | `QMark-x64` |
| `build-linux-x86.sh` | Linux i686 (cross) | `QMark-x86` |
| `build-win-x64.sh` | Windows x64 (cross) | `QMark-x64.exe` |
| `build-win-x86.sh` | Windows x86 (cross) | `QMark-x86.exe` |
| `build-all.sh` | All available targets | all of the above |
| `build-deps.sh <target>` | Build SQLite3 + libsodium | static libs in `deps/` |

Each script auto-detects dependencies in this order:

1. **Environment variables** — `QT_DIR`, `SQLITE3_DIR`, `SODIUM_DIR`, `QT_HOST_DIR`
2. **Project-local** — `deps/` directory (symlinks or real files)
3. **User home** — `~/qt6-win64`, `~/sqlite3`, `~/libsodium-win64`, etc.
4. **System-wide** — `/opt/qt6-win64`, `/opt/sqlite3`, `/opt/libsodium-win64`

---

## Dependency Setup

The `deps/` directory is the recommended way to provide dependencies. Create symlinks pointing to your actual build directories:

```bash
mkdir -p deps

# Point each symlink to where you built/installed that dependency
ln -s /your/path/to/qt6-static    deps/qt6-win64     # Qt6 static (Windows target)
ln -s /your/path/to/qt6-host      deps/qt6-host      # Qt6 host tools (moc, uic)
ln -s /your/path/to/sqlite3       deps/sqlite3       # Directory containing sqlite3.c
ln -s /your/path/to/libsodium     deps/libsodium-win64  # libsodium install prefix
```

The `deps/` directory is gitignored — it stays local to your machine.

Alternatively, set environment variables before running any build script:

```bash
QT_DIR=/my/qt6 SQLITE3_DIR=/my/sqlite3 SODIUM_DIR=/my/sodium ./build-win-x64.sh
```

---

## Supported Targets

| Target | Architecture | Compiler | Host OS |
|---|---|---|---|
| **Linux x64** | x86_64 | GCC (native) | Linux x64 |
| **Linux x86** | i686 | GCC (cross) | Linux x64 |
| **Windows x64** | x86_64 | MinGW-w64 (cross) | Linux x64 |
| **Windows x86** | i686 | MinGW-w64 (cross) | Linux x64 |

Windows targets also support native builds via MSYS2 (dynamic linking — see [MSYS2](#prerequisites-msys2--native-windows-build) and [Manual Build (MSYS2)](#msys2-native-windows)).

---

## Dependencies

| Library | Version | Purpose |
|---|---|---|
| **Qt 6** | 6.8.0+ | GUI framework (Core, Gui, Widgets, Sql) |
| **SQLite3** | 3.46.0+ | Embedded database (amalgamation) |
| **SQLiteCpp** | 3.3.0+ | C++ SQLite wrapper (vendored in repo) |
| **libsodium** | 1.0.20+ | Argon2id password hashing |

---

## Prerequisites (Linux Host)

```bash
# Build essentials
sudo apt install build-essential g++ cmake make

# Qt 6 development (for host tools / native builds)
sudo apt install qt6-base-dev libqt6sql6-sqlite qmake6

# Cross-compilers for Windows targets
sudo apt install mingw-w64        # provides x86_64-w64-mingw32-g++ and i686-w64-mingw32-g++

# For Linux x86 cross-compilation
sudo apt install gcc-multilib g++-multilib

# SQLite3 development (for Linux native builds)
sudo apt install libsqlite3-dev

# libsodium development (for Linux native builds)
sudo apt install libsodium-dev
```

---

## Prerequisites (MSYS2 — Native Windows Build)

Download and install [MSYS2](https://www.msys2.org/), then from the **MSYS2 MinGW 64-bit** terminal (not the plain MSYS2 shell):

```bash
pacman -S mingw-w64-x86_64-gcc          # MinGW-w64 gcc/g++ toolchain
pacman -S mingw-w64-x86_64-make         # provides mingw32-make.exe
pacman -S mingw-w64-x86_64-qt6-base     # Qt6 libraries + qmake6, moc, uic, rcc, windeployqt6
pacman -S mingw-w64-x86_64-sqlite3
pacman -S mingw-w64-x86_64-libsodium
```

**Notes:**

- All Qt build tools ship inside `mingw-w64-x86_64-qt6-base`: `qmake6`, `moc`, `uic`, `rcc` and `windeployqt6` are installed under `/mingw64/`. No separate tools package is required.
- No `-I`/`-L` flags and no `SQLITE_LIBDIR`/`SODIUM_LIBDIR` environment variables are needed — the packages install headers into `/mingw64/include` and libraries into `/mingw64/lib`, both of which are on the default MinGW-w64 compiler search path. This also means `SQLiteCpp`, `sqlite3`, and `sodium` are all satisfied from the packages.
- `mingw-w64-x86_64-cmake` and `mingw-w64-x86_64-qt6-svg` are **not** needed: QMark builds with qmake, and the app has no SVG resources (SVG plugins are only imported in fully static builds).
- **Dynamic, not static**: MSYS2's official Qt6/SQLite3/libsodium packages are shared libraries (DLLs), so a native MSYS2 build links dynamically and needs those DLLs at runtime — see [Manual Build (MSYS2)](#msys2-native-windows). For a single static `.exe`, use the Linux cross-compile route (`./build-win-x64.sh`).

---

## Building Static Dependencies

### SQLite3 (Static)

Download the SQLite3 amalgamation and compile as a static library:

```bash
wget https://www.sqlite.org/2024/sqlite-amalgamation-3460100.zip
unzip sqlite-amalgamation-3460100.zip
cd sqlite-amalgamation-3460100

# Native Linux
gcc -c sqlite3.c -o sqlite3.o -O2 -DSQLITE_THREADSAFE=1 -fPIC
ar rcs libsqlite3.a sqlite3.o

# Windows x64 (cross-compile)
x86_64-w64-mingw32-gcc -c sqlite3.c -o sqlite3.o -O2 -DSQLITE_THREADSAFE=1
x86_64-w64-mingw32-ar rcs libsqlite3.a sqlite3.o

# Windows x86 (cross-compile)
i686-w64-mingw32-gcc -c sqlite3.c -o sqlite3.o -O2 -DSQLITE_THREADSAFE=1
i686-w64-mingw32-ar rcs libsqlite3.a sqlite3.o
```

### libsodium (Static)

```bash
wget https://github.com/jedisct1/libsodium/releases/download/1.0.20-RELEASE/libsodium-1.0.20.tar.gz
tar xf libsodium-1.0.20.tar.gz
cd libsodium-1.0.20

# Native Linux
./configure --prefix=/opt/libsodium-linux64 --disable-shared --enable-static
make -j$(nproc) && make install

# Windows x64 (cross-compile)
./configure --host=x86_64-w64-mingw32 --prefix=/opt/libsodium-win64 --disable-shared --enable-static
make -j$(nproc) && make install

# Windows x86 (cross-compile)
./configure --host=i686-w64-mingw32 --prefix=/opt/libsodium-win32 --disable-shared --enable-static
make -j$(nproc) && make install
```

### Qt 6 (Static)

For completely static builds, Qt must be built from source with `-static`.

#### Cross-Compile from Linux (Recommended)

**Step 1 — Download Qt source:**

```bash
wget https://download.qt.io/archive/qt/6.8/6.8.0/single/qt-everywhere-src-6.8.0.tar.xz
tar xf qt-everywhere-src-6.8.0.tar.xz
```

**Step 2 — Build host tools (native Linux):**

```bash
cd qt-everywhere-src-6.8.0
mkdir build-host && cd build-host

../configure \
    -prefix /opt/qt6-host \
    -static \
    -opensource -confirm-license \
    -nomake tests -nomake examples \
    -skip qt3d -skip qt5compat -skip qtactiveqt -skip qtcharts \
    -skip qtcoap -skip qtconnectivity -skip qtdatavis3d \
    -skip qtdeclarative -skip qtdoc -skip qtgraphs -skip qtgrpc \
    -skip qthttpserver -skip qtlanguageserver -skip qtlocation \
    -skip qtlottie -skip qtmqtt -skip qtmultimedia -skip qtnetworkauth \
    -skip qtopcua -skip qtpositioning -skip qtquick3d \
    -skip qtquick3dphysics -skip qtquickeffectmaker -skip qtquicktimeline \
    -skip qtremoteobjects -skip qtscxml -skip qtsensors -skip qtserialbus \
    -skip qtserialport -skip qtshadertools -skip qtspeech -skip qttools \
    -skip qttranslations -skip qtvirtualkeyboard -skip qtwayland \
    -skip qtwebchannel -skip qtwebengine -skip qtwebsockets -skip qtwebview \
    -no-opengl -no-dbus

cmake --build . --parallel $(nproc)
cmake --install .
```

**Step 3 — Create cross-compilation toolchain file** (`mingw-x86_64.cmake`):

```cmake
set(CMAKE_SYSTEM_NAME Windows)
set(CMAKE_SYSTEM_PROCESSOR x86_64)
set(CMAKE_SYSTEM_VERSION 10.0)
set(CMAKE_C_COMPILER x86_64-w64-mingw32-gcc)
set(CMAKE_CXX_COMPILER x86_64-w64-mingw32-g++)
set(CMAKE_RC_COMPILER x86_64-w64-mingw32-windres)
set(CMAKE_FIND_ROOT_PATH /usr/x86_64-w64-mingw32)
set(CMAKE_FIND_ROOT_PATH_MODE_PROGRAM NEVER)
set(CMAKE_FIND_ROOT_PATH_MODE_LIBRARY ONLY)
set(CMAKE_FIND_ROOT_PATH_MODE_INCLUDE ONLY)
```

**Step 4 — Cross-compile Qt for Windows:**

```bash
cd qt-everywhere-src-6.8.0
mkdir build-win64 && cd build-win64

../configure \
    -prefix /opt/qt6-win64 \
    -static \
    -opensource -confirm-license \
    -xplatform win32-g++ \
    -device-option CROSS_COMPILE=x86_64-w64-mingw32- \
    -qt-host-path /opt/qt6-host \
    -nomake tests -nomake examples \
    -skip qt3d -skip qt5compat -skip qtactiveqt -skip qtcharts \
    -skip qtcoap -skip qtconnectivity -skip qtdatavis3d \
    -skip qtdeclarative -skip qtdoc -skip qtgraphs -skip qtgrpc \
    -skip qthttpserver -skip qtlanguageserver -skip qtlocation \
    -skip qtlottie -skip qtmqtt -skip qtmultimedia -skip qtnetworkauth \
    -skip qtopcua -skip qtpositioning -skip qtquick3d \
    -skip qtquick3dphysics -skip qtquickeffectmaker -skip qtquicktimeline \
    -skip qtremoteobjects -skip qtscxml -skip qtsensors -skip qtserialbus \
    -skip qtserialport -skip qtshadertools -skip qtspeech -skip qttools \
    -skip qttranslations -skip qtvirtualkeyboard -skip qtwayland \
    -skip qtwebchannel -skip qtwebengine -skip qtwebsockets -skip qtwebview \
    -no-opengl -no-dbus \
    -- -DCMAKE_TOOLCHAIN_FILE=/path/to/mingw-x86_64.cmake

cmake --build . --parallel $(nproc)
cmake --install .
```

For Windows x86, replace `x86_64` with `i686` throughout.

#### Native MSYS2 Build (Windows Only)

MSYS2's official Qt6 packages are **shared libraries** (DLLs) — there is no pre-built static Qt6 in the MSYS2 repos, so a native MSYS2 build is dynamic by default (see [Manual Build (MSYS2)](#msys2-native-windows)). For a static MSYS2 build you must compile Qt from source inside MSYS2 with `-static`, using the same configure options as above minus `-xplatform`, `-device-option`, and `-qt-host-path`, then build with `qmake6 QMark.pro STATIC_BUILD=1`. The recommended route for static Windows binaries remains the Linux cross-compile (`./build-win-x64.sh`).

---

## Manual Build (Without Scripts)

### Linux x64 (Native)

```bash
cd src
qmake6 QMark.pro \
    QMAKE_CFLAGS="-static" \
    QMAKE_CXXFLAGS="-static" \
    QMAKE_LFLAGS="-static -static-libgcc -static-libstdc++"
make -j$(nproc)
```

### Windows x64 (Cross-Compile)

```bash
cd src
x86_64-w64-mingw32-qmake6 QMark.pro \
    INCLUDEPATH+=/opt/sqlite3/include \
    INCLUDEPATH+=/opt/libsodium-win64/include \
    INCLUDEPATH+=/opt/qt6-win64/include \
    LIBS+=-L/opt/sqlite3 -lsqlite3 \
    LIBS+=-L/opt/libsodium-win64/lib -lsodium \
    LIBS+=-L/opt/qt6-win64/lib \
    QMAKE_LFLAGS+="-static -static-libgcc -static-libstdc++"
make -j$(nproc)
```

### MSYS2 (Native Windows)

From the **MSYS2 MinGW 64-bit** terminal:

```bash
cd src
qmake6 QMark.pro
mingw32-make -j$(nproc)     # 'mingw32-make' is provided by mingw-w64-x86_64-make
```

- Prefer a bare `make` instead? Install MSYS2's own make package first: `pacman -S make`, then use `make -j$(nproc)`.
- The `SUBDIRS` project builds the vendored SQLiteCpp static library first, then links the app against it. The result is `src/app/QMark.exe`.
- **Running**: launch `QMark.exe` from the MSYS2 MinGW 64-bit terminal so `/mingw64/bin` (which holds the Qt6 and MinGW-w64 runtime DLLs) is on `PATH`.
- **Distributing**: this build is dynamic. Deploy with `/mingw64/bin/windeployqt6.exe QMark.exe` and copy the MinGW-w64 runtime DLLs (`libgcc_s_seh-1.dll`, `libstdc++-6.dll`, `libwinpthread-1.dll`) next to the exe. For a single-file static build, use `./build-win-x64.sh` on Linux instead.

---

## Static Linking Details

### Required Flags

```
-static                    Static linking
-static-libgcc            No libgcc_s DLL
-static-libstdc++         No libstdc++ DLL
```

### Required Static Libraries

| Library | Static File | Notes |
|---|---|---|
| Qt 6 | `libQt6*.a` | Must build Qt from source with `-static` |
| Qt Windows Platform Plugin | `plugins/platforms/libqwindows.a` | **Required on Windows** |
| Qt SQLite Driver Plugin | `plugins/sqldrivers/libqsqlite.a` | Required for Qt SQL |
| Qt Image Format Plugins | `plugins/imageformats/libq*.a` | gif, ico, jpeg, svg, etc. |
| Qt SVG Icon Engine | `plugins/iconengines/libqsvgicon.a` | Required for SVG icons |
| Qt Modern Windows Style | `plugins/styles/libqmodernwindowsstyle.a` | Native Windows look |
| SQLite3 | `libsqlite3.a` | Build from amalgamation |
| SQLiteCpp | `libSQLiteCpp.a` | Built by build scripts from vendored source |
| libsodium | `libsodium.a` | Build with `--disable-shared --enable-static` |

### Static Plugin Linking (Windows)

Static Qt requires plugins to be explicitly imported. `main.cpp` includes:

```cpp
#include <QtPlugin>
#ifdef QT_STATICPLUGIN
Q_IMPORT_PLUGIN(QWindowsIntegrationPlugin)
Q_IMPORT_PLUGIN(QSQLiteDriverPlugin)
Q_IMPORT_PLUGIN(QGifPlugin)
Q_IMPORT_PLUGIN(QICOPlugin)
Q_IMPORT_PLUGIN(QJpegPlugin)
Q_IMPORT_PLUGIN(QSvgPlugin)
Q_IMPORT_PLUGIN(QSvgIconPlugin)
Q_IMPORT_PLUGIN(QModernWindowsStylePlugin)
#endif
```

Compile with `-DQT_STATICPLUGIN`. Use `-Wl,--start-group` / `-Wl,--end-group` around all Qt and plugin libraries to resolve circular dependencies.

Additional Windows SDK import libraries:
`-ld3d11 -ld3d12 -ldxgi -ldwrite -lsetupapi -ld3d9 -lshcore -lwtsapi32 -lauthz -lmincore -lntdll -lnetapi32 -luserenv -ldbghelp`

---

## Packaging

### Windows

After building, the only file needed is `QMark.exe`. No DLLs are required.

```bash
zip QMark-Windows-x64-$(date +%Y%m%d).zip QMark-x64.exe
```

> Native MSYS2 builds are dynamic: deploy with `/mingw64/bin/windeployqt6.exe QMark.exe` and copy the MinGW-w64 runtime DLLs (`libgcc_s_seh-1.dll`, `libstdc++-6.dll`, `libwinpthread-1.dll`) alongside the exe. See [Manual Build (MSYS2)](#msys2-native-windows).

### Linux

The static binary can be distributed as a single file:

```bash
tar czf QMark-Linux-x64-$(date +%Y%m%d).tar.gz QMark-x64
```

---

## Running

1. Place the binary in any directory
2. On first launch, `items.db` and `users.db` are created in the current directory
3. The app automatically detects no users exist and prompts you to create a **SuperAdmin** account
4. Log in with your new credentials
5. Use **Tools → Database Selection** to configure paths if needed

---

## Troubleshooting

### "Cannot find -lQt6Core" or similar

Qt static libraries not found. The build scripts search `deps/`, `~/`, and `/opt/`. Set `QT_DIR`:

```bash
QT_DIR=/your/path/to/qt6-static ./build-win-x64.sh
```

### "libgcc_s_sjlj-1.dll not found" (Windows)

Binary was not statically linked. Use the build scripts (they include `-static-libgcc -static-libstdc++`).

### "ui_mainwindow.h: No such file"

The `uic` tool wasn't found or failed. Set `QT_HOST_DIR` to your Qt host tools prefix.

### Cross-compile: "tchar.h: No such file"

MinGW cross-compiler sysroot not configured. Ensure `mingw-w64` is installed.

### Qt configure: "Qt6HostInfo not found"

Build Qt host tools (Step 2) before cross-compiling for target (Step 4).

### MSYS2: "make: command not found" / "qmake: command not found"

Run from the **MSYS2 MinGW 64-bit** terminal — its `PATH` starts with `/mingw64/bin`. The Qt6 qmake is `qmake6`; the MinGW make is `mingw32-make` (or `pacman -S make` for a bare `make`).

### MSYS2: "warning: /usr/include/sodium: No such file or directory"

Harmless. The `app.pro` default include path targets Linux; on MSYS2, `sodium.h` is found automatically via `/mingw64/include`. To silence the warning, export the paths before running qmake:

```bash
export SODIUM_INCLUDE=/mingw64/include
export SODIUM_LIBDIR=/mingw64/lib
qmake6 QMark.pro
```

### MSYS2 build runs, but Windows reports "Qt6Widgets.dll not found" when double-clicking the exe

The MSYS2 build is dynamic. Run it from the MinGW 64-bit terminal, or deploy with `windeployqt6` plus the MinGW runtime DLLs (see [Manual Build (MSYS2)](#msys2-native-windows)).

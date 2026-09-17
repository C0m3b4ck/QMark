# Build Instructions for QMark

> All builds are **completely statically linked** — no external DLLs or shared libraries required.

---

## Supported Targets

| Target | Architecture | Compiler | Host OS |
|---|---|---|---|
| **Linux x64** | x86_64 | GCC (native) | Linux x64 |
| **Linux x86** | i686 | GCC (cross) | Linux x64 |
| **Windows x64** | x86_64 | MinGW-w64 (cross) | Linux x64 |
| **Windows x86** | i686 | MinGW-w64 (cross) | Linux x64 |

Windows targets also support native builds via MSYS2.

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

Download and install [MSYS2](https://www.msys2.org/), then from the **MSYS2 MinGW 64-bit** terminal:

```bash
pacman -S mingw-w64-x86_64-gcc mingw-w64-x86_64-cmake mingw-w64-x86_64-make
pacman -S mingw-w64-x86_64-qt6-base mingw-w64-x86_64-qt6-svg
pacman -S mingw-w64-x86_64-sqlite3 mingw-w64-x86_64-libsodium
```

For MSYS2 MinGW 32-bit (x86):

```bash
pacman -S mingw-w64-i686-gcc mingw-w64-i686-cmake mingw-w64-i686-make
pacman -S mingw-w64-i686-qt6-base mingw-w64-i686-qt6-svg
pacman -S mingw-w64-i686-sqlite3 mingw-w64-i686-libsodium
```

---

## Building Static Libraries (Required for All Targets)

### SQLite3 (Static)

Download the SQLite3 amalgamation and compile as a static library:

```bash
# Download
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

#### Option A: Cross-Compile from Linux (Recommended)

This is the recommended approach — build Qt host tools natively, then cross-compile for Windows.

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

**Step 3 — Create cross-compilation toolchain file:**

Create `mingw-x86_64.cmake`:

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
set(CMAKE_FIND_ROOT_PATH_MODE_PACKAGE ONLY)
```

For x86 (i686):

```cmake
set(CMAKE_SYSTEM_NAME Windows)
set(CMAKE_SYSTEM_PROCESSOR i686)
set(CMAKE_SYSTEM_VERSION 5.1)

set(CMAKE_C_COMPILER i686-w64-mingw32-gcc)
set(CMAKE_CXX_COMPILER i686-w64-mingw32-g++)
set(CMAKE_RC_COMPILER i686-w64-mingw32-windres)

set(CMAKE_FIND_ROOT_PATH /usr/i686-w64-mingw32)
set(CMAKE_FIND_ROOT_PATH_MODE_PROGRAM NEVER)
set(CMAKE_FIND_ROOT_PATH_MODE_LIBRARY ONLY)
set(CMAKE_FIND_ROOT_PATH_MODE_INCLUDE ONLY)
set(CMAKE_FIND_ROOT_PATH_MODE_PACKAGE ONLY)
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

#### Option B: Native MSYS2 Build (Windows Only)

From MSYS2 MinGW terminal, Qt is available as a pre-built package. For static builds, install the `-static` variants if available, or build Qt from source within MSYS2.

---

## Building QMark

### Linux x64 (Native Build)

```bash
cd src
qmake6 QMark.pro \
    QMAKE_CFLAGS="-static" \
    QMAKE_CXXFLAGS="-static" \
    QMAKE_LFLAGS="-static -static-libgcc -static-libstdc++"
make -j$(nproc)
```

The binary will be in `src/app/QMark`.

### Linux x86 (Cross-Compile from x64)

```bash
# Install 32-bit libraries
sudo dpkg --add-architecture i386
sudo apt update
sudo apt install libsqlite3-dev:i386 libsodium-dev:i386

cd src
qmake6 QMark.pro \
    QMAKE_CC=gcc \
    QMAKE_CXX=g++ \
    QMAKE_CFLAGS="-m32 -static" \
    QMAKE_CXXFLAGS="-m32 -static" \
    QMAKE_LFLAGS="-m32 -static -static-libgcc -static-libstdc++"
make -j$(nproc)
```

### Windows x64 (Cross-Compile from Linux)

Create a `win64-static.pro` include or pass flags directly:

```bash
cd src

# Build SQLiteCpp static library
cd sqlitecpp
x86_64-w64-mingw32-qmake6 sqlitecpp.pro
make -j$(nproc)
cd ..

# Build QMark
x86_64-w64-mingw32-qmake6 QMark.pro \
    INCLUDEPATH+=/opt/sqlite3/include \
    INCLUDEPATH+=/opt/libsodium-win64/include \
    INCLUDEPATH+=/opt/qt6-win64/include \
    LIBS+=-L/opt/sqlite3 -lsqlite3 \
    LIBS+=-L/opt/libsodium-win64/lib -lsodium \
    LIBS+=-L/opt/qt6-win64/lib \
    QMAKE_CFLAGS_STATIC_WIN="-static" \
    QMAKE_LFLAGS+="-static -static-libgcc -static-libstdc++"
make -j$(nproc)
```

The binary will be in `src/app/QMark.exe`.

### Windows x86 (Cross-Compile from Linux)

Same as x64 but use `i686-w64-mingw32-` toolchain prefix and x86 paths.

### Native Windows Build (MSYS2)

From MSYS2 MinGW terminal:

```bash
cd src
qmake QMark.pro
make -j$(nproc)
```

For static linking in MSYS2, add static flags to the `.pro` file or pass them via qmake.

---

## Static Linking Flags Summary

For a **completely static** binary, the following flags must be applied:

### g++ / MinGW (linker flags)

```
-static                    # Static linking
-static-libgcc            # Static libgcc (no libgcc_s DLL)
-static-libstdc++         # Static libstdc++ (no libstdc++ DLL)
-static-runtime           # (MSVC only) Static C runtime
```

### qmake Variables

```
QMAKE_LFLAGS += -static -static-libgcc -static-libstdc++
```

### Required Static Libraries

| Library | Static File | Notes |
|---|---|---|
| Qt 6 | `libQt6*.a` | Must build Qt from source with `-static` |
| SQLite3 | `libsqlite3.a` | Build from amalgamation |
| SQLiteCpp | `libSQLiteCpp.a` | Build from vendored source |
| libsodium | `libsodium.a` | Build with `--disable-shared --enable-static` |
| libgcc | (built-in) | Use `-static-libgcc` |
| libstdc++ | (built-in) | Use `-static-libstdc++` |

---

## Packaging

### Windows

After building, the only file needed is `QMark.exe`. No DLLs are required.

```bash
# Create release zip
cd src/app
zip QMark-Windows-x64-$(date +%Y%m%d).zip QMark.exe
```

### Linux

The static binary can be distributed as a single file. Optionally create a tarball:

```bash
cd src/app
tar czf QMark-Linux-x64-$(date +%Y%m%d).tar.gz QMark
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

Qt static libraries are not installed or not in the library path. Verify with:

```bash
ls /opt/qt6-win64/lib/libQt6Core.a    # Windows cross-compile
ls /opt/qt6-host/lib/libQt6Core.a     # Linux native
```

### "libgcc_s_sjlj-1.dll not found" (Windows)

The binary was not statically linked. Add `-static-libgcc -static-libstdc++` to linker flags.

### "sqlite3.h: No such file"

SQLite3 headers are not in the include path. Add `-I/path/to/sqlite3` to `INCLUDEPATH`.

### Cross-compile: "tchar.h: No such file or directory"

Ensure the MinGW cross-compiler sysroot is correctly configured. The toolchain file should set `CMAKE_FIND_ROOT_PATH` to the MinGW sysroot.

### Qt configure: "Qt6HostInfo not found"

When cross-compiling, Qt needs host tools built first. Follow Step 2 (build host tools) before Step 4 (cross-compile for target) in the Qt static build instructions above.

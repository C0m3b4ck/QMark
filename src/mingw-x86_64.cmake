# CMake toolchain file for cross-compiling QMark to Windows x64 from Linux
#
# Usage:
#   cmake -DCMAKE_TOOLCHAIN_FILE=mingw-x86_64.cmake ..
#
# Or with Qt's configure:
#   ../configure ... -- -DCMAKE_TOOLCHAIN_FILE=/path/to/mingw-x86_64.cmake

set(CMAKE_SYSTEM_NAME Windows)
set(CMAKE_SYSTEM_PROCESSOR x86_64)
set(CMAKE_SYSTEM_VERSION 10.0)

# Cross-compiler
set(CMAKE_C_COMPILER x86_64-w64-mingw32-gcc)
set(CMAKE_CXX_COMPILER x86_64-w64-mingw32-g++)
set(CMAKE_RC_COMPILER x86_64-w64-mingw32-windres)
set(CMAKE_ASM_COMPILER x86_64-w64-mingw32-gcc)

# Search paths — only look in the cross environment
set(CMAKE_FIND_ROOT_PATH /usr/x86_64-w64-mingw32)
set(CMAKE_FIND_ROOT_PATH_MODE_PROGRAM NEVER)
set(CMAKE_FIND_ROOT_PATH_MODE_LIBRARY ONLY)
set(CMAKE_FIND_ROOT_PATH_MODE_INCLUDE ONLY)
set(CMAKE_FIND_ROOT_PATH_MODE_PACKAGE ONLY)

# Static linking flags
set(CMAKE_EXE_LINKER_FLAGS "-static -static-libgcc -static-libstdc++"
    CACHE STRING "Linker flags" FORCE)
set(CMAKE_SHARED_LINKER_FLAGS "-static -static-libgcc -static-libstdc++"
    CACHE STRING "Shared linker flags" FORCE)

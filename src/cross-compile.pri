# Cross-compilation toolchain for QMark
# Include this file in your qmake invocation:
#   qmake6 QMark.pro STATIC_BUILD=1 "CROSS_PREFIX=x86_64-w64-mingw32-"

# --- Cross-compiler settings ---
!isEmpty(CROSS_PREFIX) {
    QMAKE_CC = $${CROSS_PREFIX}gcc
    QMAKE_CXX = $${CROSS_PREFIX}g++
    QMAKE_LINK = $${CROSS_PREFIX}g++
    QMAKE_LINK_C = $${CROSS_PREFIX}gcc
    QMAKE_AR = $${CROSS_PREFIX}ar
    QMAKE_RANLIB = $${CROSS_PREFIX}ranlib
    QMAKE_STRIP = $${CROSS_PREFIX}strip
    QMAKE_RC = $${CROSS_PREFIX}windres
    QMAKE_OBJCOPY = $${CROSS_PREFIX}objcopy
}

# --- Static linking flags ---
contains(STATIC_BUILD, 1) {
    QMAKE_CFLAGS += -static
    QMAKE_CXXFLAGS += -static
    QMAKE_LFLAGS += -static -static-libgcc -static-libstdc++
}

# Cross-compilation toolchain for Windows x64 (MinGW)
# Usage: qmake6 -spec win32-g++ QMark.pro

QT_WIN_PREFIX = /tmp/qt6-win/6.8.0/mingw_64
SQLITE_PATH = /tmp/sqlite-amalgamation-3460100
SODIUM_PATH = /tmp/libsodium-win

INCLUDEPATH += $$SQLITE_PATH
INCLUDEPATH += $$SODIUM_PATH/include
INCLUDEPATH += $$QT_WIN_PREFIX/include
INCLUDEPATH += $$QT_WIN_PREFIX/include/QtCore
INCLUDEPATH += $$QT_WIN_PREFIX/include/QtWidgets
INCLUDEPATH += $$QT_WIN_PREFIX/include/QtGui
INCLUDEPATH += $$QT_WIN_PREFIX/include/QtSql

LIBS += -L$$SQLITE_PATH -lsqlite3
LIBS += -L$$SODIUM_PATH/lib -lsodium
LIBS += -L$$QT_WIN_PREFIX/lib

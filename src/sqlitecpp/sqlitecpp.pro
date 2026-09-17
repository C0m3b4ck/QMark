TEMPLATE = lib
CONFIG += staticlib
CONFIG += c++17
TARGET = SQLiteCpp

SQLITE_PATH = ../../sqlitecpp

INCLUDEPATH += $$SQLITE_PATH/include

SOURCES += \
    $$SQLITE_PATH/src/Backup.cpp \
    $$SQLITE_PATH/src/Column.cpp \
    $$SQLITE_PATH/src/Database.cpp \
    $$SQLITE_PATH/src/Exception.cpp \
    $$SQLITE_PATH/src/Savepoint.cpp \
    $$SQLITE_PATH/src/Statement.cpp \
    $$SQLITE_PATH/src/Transaction.cpp

HEADERS += \
    $$SQLITE_PATH/include/SQLiteCpp/Database.h \
    $$SQLITE_PATH/include/SQLiteCpp/Column.h \
    $$SQLITE_PATH/include/SQLiteCpp/Statement.h \
    $$SQLITE_PATH/include/SQLiteCpp/Transaction.h \
    $$SQLITE_PATH/include/SQLiteCpp/Backup.h \
    $$SQLITE_PATH/include/SQLiteCpp/Exception.h \
    $$SQLITE_PATH/include/SQLiteCpp/Savepoint.h \
    $$SQLITE_PATH/include/SQLiteCpp/VariadicBind.h \
    $$SQLITE_PATH/include/SQLiteCpp/SQLiteCpp.h

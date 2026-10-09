TEMPLATE = app
CONFIG += c++17
TARGET = QMark

QT += widgets sql network

# --- Cross-compilation / static build paths ---
# Override these via qmake args or environment:
#   qmake6 QMark.pro CROSS_PREFIX=x86_64-w64-mingw32-
# Or set environment variables:
#   SODIUM_INCLUDE, SODIUM_LIBDIR, SQLITE_LIBDIR

CROSS_PREFIX = $$getenv(CROSS_PREFIX)
SODIUM_INCDIR = $$getenv(SODIUM_INCLUDE)
SODIUM_LIBDIR = $$getenv(SODIUM_LIBDIR)
SQLITE_INCDIR = $$getenv(SQLITE_INCLUDE)
SQLITE_LIBDIR = $$getenv(SQLITE_LIBDIR)

INCLUDEPATH += ../../sqlitecpp/include
DEPENDPATH += ../../sqlitecpp/include

!isEmpty(SODIUM_INCDIR): INCLUDEPATH += $$SODIUM_INCDIR
else: INCLUDEPATH += /usr/include/sodium

# SQLiteCpp is built as a static lib by the subdirs project
LIBS += -L$$OUT_PWD/../sqlitecpp -lSQLiteCpp

# Link SQLite3 statically if a custom libdir is provided, otherwise use system
!isEmpty(SQLITE_LIBDIR) {
    LIBS += -L$$SQLITE_LIBDIR -lsqlite3
} else {
    LIBS += -lsqlite3
}

# Link libsodium statically if a custom libdir is provided, otherwise use system
!isEmpty(SODIUM_LIBDIR) {
    LIBS += -L$$SODIUM_LIBDIR -lsodium
} else {
    LIBS += -lsodium
}

# --- Static linking flags (for fully static builds) ---
contains(STATIC_BUILD, 1) {
    QMAKE_LFLAGS += -static -static-libgcc -static-libstdc++
}

SOURCES += \
    backup.cpp \
    businesslogic.cpp \
    crypto.cpp \
    main.cpp \
    mainwindow.cpp \
    netutil.cpp \
    remoteserver.cpp \
    sanitize_string.cpp \
    scannerdetect.cpp \
    smtpclient.cpp \
    sqlite_dataaccess.cpp \
    summaryreport.cpp \
    updater.cpp \
    zipwriter.cpp

HEADERS += \
    appsettings.h \
    backup.h \
    businesslogic.h \
    charts.h \
    crypto.h \
    dataaccess.h \
    domain.h \
    logger.h \
    mainwindow.h \
    netutil.h \
    pdf_export.h \
    remoteserver.h \
    scannerdetect.h \
    smtpclient.h \
    sqlite_dataaccess.h \
    statistics.h \
    summaryreport.h \
    telemetry.h \
    translations.h \
    updater.h \
    version.h \
    worklog.h \
    zipwriter.h

FORMS += \
    mainwindow.ui

# Default rules for deployment.
qnx: target.path = /tmp/$${TARGET}/bin
else: unix:!android: target.path = /opt/$${TARGET}/bin
!isEmpty(target.path): INSTALLS += target

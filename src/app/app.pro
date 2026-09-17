TEMPLATE = app
CONFIG += c++17
TARGET = QMark

QT += widgets sql

INCLUDEPATH += ../../sqlitecpp/include
INCLUDEPATH += /usr/include/sodium
DEPENDPATH += ../../sqlitecpp/include

LIBS += -L../sqlitecpp -lSQLiteCpp -lsqlite3
LIBS += -lsodium

SOURCES += \
    businesslogic.cpp \
    crypto.cpp \
    main.cpp \
    mainwindow.cpp \
    sanitize_string.cpp \
    sqlite_dataaccess.cpp

HEADERS += \
    businesslogic.h \
    crypto.h \
    dataaccess.h \
    domain.h \
    logger.h \
    mainwindow.h \
    sqlite_dataaccess.h \
    telemetry.h \
    worklog.h

FORMS += \
    mainwindow.ui

# Default rules for deployment.
qnx: target.path = /tmp/$${TARGET}/bin
else: unix:!android: target.path = /opt/$${TARGET}/bin
!isEmpty(target.path): INSTALLS += target

TEMPLATE = subdirs
CONFIG += ordered

SUBDIRS = \
    sqlitecpp \
    app

app.depends = sqlitecpp

# Pass static build flag to subprojects
contains(STATIC_BUILD, 1) {
    QMAKE_EXTRA_VARIABLES += STATIC_BUILD=1
}

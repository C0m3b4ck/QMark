#include "mainwindow.h"
#include "sqlite_dataaccess.h"
#include "logger.h"
#include "telemetry.h"
#include "businesslogic.h"
#include <QApplication>
#include <QDateTime>
#include <QSettings>
#include <QDir>
#include <cstdlib>

#ifdef QT_STATICPLUGIN
Q_IMPORT_PLUGIN(QWindowsIntegrationPlugin)
Q_IMPORT_PLUGIN(QSQLiteDriverPlugin)
Q_IMPORT_PLUGIN(QGifPlugin)
Q_IMPORT_PLUGIN(QICOPlugin)
Q_IMPORT_PLUGIN(QJpegPlugin)
Q_IMPORT_PLUGIN(QWindowsVistaStylePlugin)
#endif

static void cleanupLogger() {
    telemetry().close();
    AppLogger::instance().close();
}

int main(int argc, char *argv[])
{
    QApplication a(argc, argv);

    // Application metadata
    a.setOrganizationName("QMark");
    a.setApplicationName("QMarkSchoolShop");
    a.setApplicationDisplayName("QMark — School Shop PoS");

    std::atexit(cleanupLogger);
    QObject::connect(&a, &QCoreApplication::aboutToQuit, &cleanupLogger);

    // Telemetry via CLI flag
    if (a.arguments().contains("--telemetry")) {
        QString logDir = QCoreApplication::applicationDirPath();
        QString csvPath = logDir + "/telemetry_" + QDateTime::currentDateTime().toString("yyyyMMdd_hhmmss") + ".csv";
        QString dbPath  = logDir + "/telemetry_" + QDateTime::currentDateTime().toString("yyyyMMdd_hhmmss") + ".db";
        AppLogger::instance().setTelemetryEnabled(true);
        AppLogger::instance().setLogFile(logDir + "/telemetry_" + QDateTime::currentDateTime().toString("yyyyMMdd_hhmmss") + ".log");
        telemetry().open(csvPath, dbPath);
        qDebug() << "[TELEMETRY] Enabled via CLI flag, CSV:" << csvPath << " DB:" << dbPath;
    }

    // Load default databases from saved settings, or create new ones
    auto& db = DataAccess::SQLiteDataAccess::instance();
    QSettings settings("QMark", "SchoolShop");
    QString itemsDbPath = settings.value("db/itemsPath", QDir::currentPath() + "/items.db").toString();
    QString usersDbPath = settings.value("db/usersPath", QDir::currentPath() + "/users.db").toString();

    BusinessLogic::initializeCrypto();

    if (!BusinessLogic::initializeDatabases(db, itemsDbPath.toStdString(), usersDbPath.toStdString())) {
        qDebug() << "[MAIN] Warning: Could not connect to databases on first try.";
    }

    MainWindow w(db);

    // Maximized = fits to screen (works on PC and tablets)
    w.showMaximized();

    return QApplication::exec();
}

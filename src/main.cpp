#include "mainwindow.h"
#include "sqlite_dataaccess.h"
#include "logger.h"
#include "telemetry.h"
#include "businesslogic.h"
#include <QApplication>
#include <QDateTime>
#include <QSettings>
#include <QDir>
#include <QStyleFactory>
#include <QPalette>
#include <QColor>
#include <QtPlugin>
#include <cstdlib>

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

static void cleanupLogger() {
    telemetry().close();
    AppLogger::instance().close();
}

int main(int argc, char *argv[])
{
    QApplication a(argc, argv);

    // ── Force Fusion style + explicit light palette ─────────────────
    // Fixes "invisible" (white-on-white) text on some platforms,
    // e.g. Windows 11 with dark-mode/auto palette in the default style.
    QApplication::setStyle(QStyleFactory::create("Fusion"));
    {
        QPalette pal = a.palette();
        pal.setColor(QPalette::Window, QColor(245, 245, 245));
        pal.setColor(QPalette::WindowText, QColor(0, 0, 0));
        pal.setColor(QPalette::Base, QColor(255, 255, 255));
        pal.setColor(QPalette::AlternateBase, QColor(240, 240, 240));
        pal.setColor(QPalette::ToolTipBase, QColor(255, 255, 220));
        pal.setColor(QPalette::ToolTipText, QColor(0, 0, 0));
        pal.setColor(QPalette::Text, QColor(0, 0, 0));
        pal.setColor(QPalette::Button, QColor(230, 230, 230));
        pal.setColor(QPalette::ButtonText, QColor(0, 0, 0));
        pal.setColor(QPalette::BrightText, QColor(255, 255, 255));
        pal.setColor(QPalette::Highlight, QColor(0, 120, 215));
        pal.setColor(QPalette::HighlightedText, QColor(255, 255, 255));
        a.setPalette(pal);
    }

    // Application metadata
    a.setOrganizationName("QMark");
    a.setApplicationName("QMarkSchoolShop");
    a.setApplicationDisplayName("QMark");

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

    // Apply saved currency (default PLN). SuperAdmin can change it in Preferences.
    QString currency = settings.value("settings/currency", "PLN").toString();
    Domain::setCurrencySymbol(currency == "USD" ? "$" : "zł");

    BusinessLogic::initializeCrypto();

    if (!BusinessLogic::initializeDatabases(db, itemsDbPath.toStdString(), usersDbPath.toStdString())) {
        qDebug() << "[MAIN] Warning: Could not connect to databases on first try.";
    }

    MainWindow w(db);

    // Maximized = fits to screen (works on PC and tablets)
    w.showMaximized();

    return QApplication::exec();
}

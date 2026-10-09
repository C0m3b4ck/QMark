#include "backup.h"

#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QFileInfoList>
#include <QSettings>
#include <QBuffer>
#include <QDateTime>
#include <QUrl>

#include "appsettings.h"
#include "netutil.h"
#include "zipwriter.h"

namespace Backup {

QByteArray buildBundle()
{
    QSettings settings("QMark", "SchoolShop");
    const QString itemsPath = settings.value("db/itemsPath", QDir::currentPath() + "/items.db").toString();
    const QString usersPath = settings.value("db/usersPath", QDir::currentPath() + "/users.db").toString();

    QByteArray itemsDb, usersDb;
    QFile f;
    f.setFileName(itemsPath);
    if (f.open(QIODevice::ReadOnly)) itemsDb = f.readAll();
    f.setFileName(usersPath);
    if (f.open(QIODevice::ReadOnly)) usersDb = f.readAll();

    QByteArray summary;
    summary = "QMark backup\nGenerated: "
            + QDateTime::currentDateTime().toString("yyyy-MM-dd hh:mm:ss").toUtf8()
            + "\nitems.db: " + QByteArray::number(itemsDb.size()) + " bytes\n"
            + "users.db: " + QByteArray::number(usersDb.size()) + " bytes\n";

    QList<QPair<QString, QByteArray>> files;
    if (!itemsDb.isEmpty()) files.append({ "items.db", itemsDb });
    if (!usersDb.isEmpty()) files.append({ "users.db", usersDb });
    files.append({ "SUMMARY.txt", summary });
    return Zip::pack(files);
}

QString archiveFileName(const QDateTime& when)
{
    return "QMark-backup-" + when.toString("yyyyMMdd-hhmmss") + ".zip";
}

QString createLocal(const QString& folder, int keepCount, QString* err)
{
    if (!QDir().mkpath(folder)) {
        if (err) *err = QStringLiteral("Cannot create backup folder: %1").arg(folder);
        return QString();
    }

    const QDateTime when = QDateTime::currentDateTime();
    const QString fileName = archiveFileName(when);
    const QString path = QDir(folder).filePath(fileName);

    const QByteArray bundle = buildBundle();
    QFile out(path);
    if (!out.open(QIODevice::WriteOnly | QIODevice::Truncate)) {
        if (err) *err = QStringLiteral("Cannot write backup file: %1").arg(path);
        return QString();
    }
    out.write(bundle);
    out.close();

    // Prune old archives (timestamped names sort chronologically).
    QDir dir(folder);
    QStringList filters;
    filters << "QMark-backup-*.zip";
    QFileInfoList old = dir.entryInfoList(filters, QDir::Files, QDir::Name);
    while (old.size() > qMax(1, keepCount)) {
        QFile::remove(old.first().absoluteFilePath());
        old.removeFirst();
    }

    return path;
}

bool uploadHttp(const QString& baseUrl, const QString& token,
                const QByteArray& bundle, const QString& fileName, QString* err)
{
    if (baseUrl.isEmpty()) {
        if (err) *err = QStringLiteral("Backup server URL is not configured.");
        return false;
    }

    QString urlStr = baseUrl;
    if (!urlStr.contains("{filename}")) {
        if (!urlStr.endsWith('/')) urlStr += '/';
        urlStr += fileName;
    } else {
        urlStr.replace("{filename}", fileName);
    }
    const QUrl url(urlStr);

    Net::Headers headers;
    if (!token.isEmpty()) {
        headers.append({ "Authorization", "Bearer " + token.toUtf8() });
    }
    return Net::httpPut(url, headers, bundle, 120000, err);
}

} // namespace Backup
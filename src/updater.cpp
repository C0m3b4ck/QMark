#include "updater.h"

#include <QFile>
#include <QFileInfo>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonValue>
#include <QProcess>
#include <QCoreApplication>
#include <QDir>

#include "netutil.h"
#include "version.h"

namespace Updater {

int sequenceOf(const QString& versionOrTag)
{
    const QString t = versionOrTag.trimmed();
    // Accept "alpha_14", "alpha14", "v14", "14", "14.2", ...
    int from = 0;
    if (t.startsWith("alpha", Qt::CaseInsensitive)) {
        from = 5;
        if (t.size() > from && t.at(from) == '_') ++from;
    } else if (t.startsWith('v') || t.startsWith('V')) {
        from = 1;
    }
    bool ok = false;
    int num = t.mid(from).split('.').first().toInt(&ok);
    return ok ? num : -1;
}

bool checkLatest(UpdateInfo* out, QString* err)
{
    if (!out) return false;
    *out = UpdateInfo();

    const QUrl url(QStringLiteral("https://api.github.com/repos/%1/releases?per_page=5")
                       .arg(QStringLiteral(QMARK_GITHUB_REPO)));

    Net::Headers headers;
    headers.append(Net::Headers::value_type("Accept", "application/vnd.github+json"));

    bool ok = false;
    QString localErr;
    const QByteArray body = Net::httpGet(url, headers, 30000, &ok, &localErr);
    if (!ok) {
        if (err) *err = localErr.isEmpty() ? QStringLiteral("network error") : localErr;
        return false;
    }

    QJsonParseError perr;
    const QJsonDocument doc = QJsonDocument::fromJson(body, &perr);
    if (perr.error != QJsonParseError::NoError || !doc.isArray()) {
        if (err) *err = QStringLiteral("invalid GitHub response.");
        return false;
    }

    const QJsonArray arr = doc.array();
    if (arr.isEmpty()) {
        if (err) *err = QStringLiteral("No releases found on GitHub.");
        return false;
    }

    // Releases API returns newest-first; take the first non-draft release.
    for (const QJsonValue& v : arr) {
        const QJsonObject rel = v.toObject();
        if (rel.value("draft").toBool()) continue;
        out->tag = rel.value("tag_name").toString();
        out->prerelease = rel.value("prerelease").toBool();

        const int newSeq = sequenceOf(out->tag);
        const int curSeq = QMARK_VERSION_SEQ;
        out->newer = (newSeq > curSeq);

        // Find the platform asset.
        const QJsonArray assets = rel.value("assets").toArray();
#ifdef Q_OS_WIN
        const QString assetName = "QMark-x64.exe";
#else
        const QString assetName = "QMark-x64";
#endif
        for (const QJsonValue& a : assets) {
            const QJsonObject ao = a.toObject();
            if (ao.value("name").toString() == assetName) {
                const QUrl dl(ao.value("browser_download_url").toString());
                // Only accept an HTTPS download URL: refuse a plaintext
                // (or non-network) asset that a tampered response could inject.
                if (dl.scheme() != "https") continue;
                out->assetName = assetName;
                out->assetUrl = dl;
                out->assetSize = qint64(ao.value("size").toDouble(-1));
                break;
            }
        }
        return true;
    }

    if (err) *err = QStringLiteral("No public releases found on GitHub.");
    return false;
}

bool download(const UpdateInfo& info, const QString& destPath, QString* err)
{
    if (info.assetUrl.isEmpty()) {
        if (err) *err = QStringLiteral("Release has no %1 asset.").arg(
            #ifdef Q_OS_WIN
            "QMark-x64.exe"
            #else
            "QMark-x64"
            #endif
        );
        return false;
    }
    // The asset must be fetched over HTTPS; anything else could be a
    // downgrade injected by a tampered API response.
    if (info.assetUrl.scheme() != "https") {
        if (err) *err = QStringLiteral("Refusing to download an update over an insecure connection.");
        return false;
    }

    Net::Headers headers;
    headers.append(Net::Headers::value_type("Accept", "application/octet-stream"));
    return Net::httpDownload(info.assetUrl, headers, destPath,
                             info.assetSize, 600000, err);
}

bool apply(const QString& destPath, const QString& targetPath, QString* err)
{
    QFileInfo newFile(destPath);
    if (!newFile.exists() || newFile.size() == 0) {
        if (err) *err = QStringLiteral("Downloaded update is missing or empty.");
        return false;
    }

    const QString oldPath = targetPath + ".old";
    QFile::remove(oldPath);

    if (!QFile::rename(targetPath, oldPath)) {
        if (err) *err = QStringLiteral("Could not back up the current executable.");
        return false;
    }
    if (!QFile::rename(destPath, targetPath)) {
        // Roll back.
        if (QFile::exists(oldPath)) QFile::rename(oldPath, targetPath);
        if (err) *err = QStringLiteral("Could not install the update.");
        return false;
    }

    // Relaunch the new binary and exit.
    const QString app = targetPath;
    QProcess::startDetached(app, QCoreApplication::arguments().mid(1),
                            QCoreApplication::applicationDirPath());
    return true;
}

} // namespace Updater
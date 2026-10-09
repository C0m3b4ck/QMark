#include "updater.h"

#include <QFile>
#include <QFileInfo>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonValue>
#include <QProcess>
#include <QStringList>
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

TagVersion parseTag(const QString& tag)
{
    TagVersion v;
    v.raw = tag.trimmed();

    // Skip leading non-digits (prefixes like "alpha_", "v", "release-").
    int i = 0;
    while (i < v.raw.size() && !v.raw.at(i).isDigit()) ++i;

    // Collect digits and dots — the version "core". Anything else
    // ("-", "_", letters, spaces) terminates it, so "2.0.0-rc1" → "2.0.0".
    int j = i;
    while (j < v.raw.size() && (v.raw.at(j).isDigit() || v.raw.at(j) == '.')) ++j;

    const QString core = v.raw.mid(i, j - i);
    const QStringList parts = core.split('.', Qt::SkipEmptyParts);
    for (const QString& p : parts) {
        bool ok = false;
        const int n = p.toInt(&ok);
        if (!ok) return v;                    // malformed (e.g. "1.0.x") → lexical fallback
        v.nums.append(n);
    }
    v.valid = !v.nums.isEmpty();
    return v;
}

int compareTags(const TagVersion& a, const TagVersion& b)
{
    if (a.valid && b.valid) {
        const int n = qMax(a.nums.size(), b.nums.size());
        for (int i = 0; i < n; ++i) {
            const int x = i < a.nums.size() ? a.nums.at(i) : 0;
            const int y = i < b.nums.size() ? b.nums.at(i) : 0;
            if (x > y) return 1;
            if (x < y) return -1;
        }
        return 0;
    }
    // Tags without numbers (or mixed numeric/lexical schemes) compare
    // lexicographically so the result is always well-defined.
    const int c = QString::compare(a.raw, b.raw, Qt::CaseInsensitive);
    return c > 0 ? 1 : c < 0 ? -1 : 0;
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

        // Compare the release tag against the running build component-wise,
        // so any tag convention ("alpha_N", "v1.2.3", "1.2.10", …) is
        // understood and dot-versions don't collapse to their major number.
        out->newer = compareTags(parseTag(out->tag),
                                 parseTag(QStringLiteral(QMARK_VERSION_STRING))) > 0;

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

bool restore(const QString& targetPath, QString* err)
{
    const QString oldPath = targetPath + ".old";
    if (!QFile::exists(oldPath)) {
        if (err) *err = QStringLiteral("No previous release backup found.");
        return false;
    }

    // The current (running) executable cannot be deleted while in use, but
    // it CAN be renamed: set it aside as .tmp, then put the backup back.
    const QString tmpPath = targetPath + ".tmp";
    if (!QFile::rename(targetPath, tmpPath)) {
        if (err) *err = QStringLiteral("Could not set aside the current program.");
        return false;
    }
    if (!QFile::rename(oldPath, targetPath)) {
        // Roll back so the currently-running binary keeps its original name.
        if (QFile::exists(tmpPath)) QFile::rename(tmpPath, targetPath);
        if (err) *err = QStringLiteral("Could not restore the previous release.");
        return false;
    }

    // Relaunch the restored binary and exit. The leftover <exe>.tmp (the
    // release that was just rolled back) is removed at the next startup.
    QProcess::startDetached(targetPath, QCoreApplication::arguments().mid(1),
                            QCoreApplication::applicationDirPath());
    return true;
}

void cleanupArtifacts()
{
    // Remove leftovers of an interrupted update/restore. These can only be
    // deleted once the process that holds them has exited, hence they're
    // cleaned here, at the start of the next launch.
    const QString base = QCoreApplication::applicationFilePath();
    QFile::remove(base + ".part");   // interrupted download
    QFile::remove(base + ".tmp");    // executable set aside by a restore
}

} // namespace Updater
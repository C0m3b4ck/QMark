#pragma once

// ─────────────────────────────────────────────────────────────────────
// updater.h — GitHub Releases based auto-update.
//
// Checks the QMark GitHub releases API for the newest tag ("alpha_N"),
// compares it against the baked-in QMARK_VERSION_SEQ, optionally
// downloads the release binary (QMark-x64.exe on Windows, QMark-x64
// otherwise) into the app directory and swaps it in on the next launch.
// Toggled on/off by the SuperAdmin (update/enabled setting).
// ─────────────────────────────────────────────────────────────────────

#include <QString>
#include <QUrl>

struct UpdateInfo {
    QString tag;            // newest release tag, e.g. "alpha_14"
    QString assetName;      // "QMark-x64.exe" / "QMark-x64"
    QUrl assetUrl;
    qint64 assetSize = -1;
    bool newer = false;     // true when tag is newer than the running build
    bool prerelease = false;
};

namespace Updater {

// Parse "alpha_N" / "vN" / "N" style tags into a comparable sequence.
int sequenceOf(const QString& versionOrTag);

// Query GitHub for the latest release. newTag/sequence comparison against
// the running version happens inside; check UpdateInfo::newer.
bool checkLatest(UpdateInfo* out, QString* err);

// Download the release binary to destPath (temp file). Verifies the size
// reported by the GitHub API when available.
bool download(const UpdateInfo& info, const QString& destPath, QString* err);

// Swap the downloaded file for the running executable:
//   <target>       → <target>.old
//   <destPath>     → <target>
// then relaunches the new binary. Returns false if any step fails.
bool apply(const QString& destPath, const QString& targetPath, QString* err);

} // namespace Updater
#pragma once

// ─────────────────────────────────────────────────────────────────────
// updater.h — GitHub Releases based auto-update.
//
// Checks the QMark GitHub releases API for the newest tag, compares it
// against the baked-in running version, optionally downloads the release
// binary (QMark-x64.exe on Windows, QMark-x64 otherwise) into the app
// directory and swaps it in on the next launch. Toggled on/off by the
// SuperAdmin (update/enabled setting).
//
// Tags are version-compared by their numeric components, so any naming
// scheme works ("alpha_14", "v1.2.3", "1.2.3", "2024.10.1", "2.0.0-rc1"
// …). "14" vs "1.2.3" compare as plain numbers, so when you switch tag
// conventions the new scheme should keep counting up (e.g. alpha_13 →
// 14.0.0) for updates to keep being detected.
// ─────────────────────────────────────────────────────────────────────

#include <QString>
#include <QUrl>
#include <QVector>

struct UpdateInfo {
    QString tag;            // newest release tag, e.g. "alpha_14" or "1.2.3"
    QString assetName;      // "QMark-x64.exe" / "QMark-x64"
    QUrl assetUrl;
    qint64 assetSize = -1;
    bool newer = false;     // true when tag is newer than the running build
    bool prerelease = false;
};

namespace Updater {

// A parsed release tag: the numeric dot-components (major.minor.patch…)
// extracted from the tag plus the trimmed original for lexicographic
// fallbacks. "alpha_14" → {14}; "v1.2.3-beta.1" → {1,2,3}; "2024.08.01"
// → {2024,8,1}; "stable"/"" → valid=false.
struct TagVersion {
    QVector<int> nums;
    QString raw;
    bool valid = false;
};

TagVersion parseTag(const QString& tag);

// 1  → a newer than b;  -1 → a older than b;  0 → equal / undecidable.
// Numeric tags compare component-wise (1.2.10 > 1.2.9); tags without any
// digits fall back to a plain lexicographic comparison.
int compareTags(const TagVersion& a, const TagVersion& b);

// Parse "alpha_N" / "vN" / "N" style tags into a comparable sequence.
// Kept for parity/diagnostics: returns the FIRST integer found (so
// "1.2.3" and "1.9" both read 1 — prefer compareTags for real checks).
int sequenceOf(const QString& versionOrTag);

// Query GitHub for the latest release. The tag comparison against the
// running version happens inside; check UpdateInfo::newer.
bool checkLatest(UpdateInfo* out, QString* err);

// Download the release binary to destPath (temp file). Verifies the size
// reported by the GitHub API when available.
bool download(const UpdateInfo& info, const QString& destPath, QString* err);

// Swap the downloaded file for the running executable:
//   <target>       → <target>.old
//   <destPath>     → <target>
// then relaunches the new binary. Returns false if any step fails.
bool apply(const QString& destPath, const QString& targetPath, QString* err);

// Reverse of apply(): restore the previous release kept at <target>.old:
//   <target>       → <target>.tmp        (set the running exe aside)
//   <target>.old   → <target>            (previous release back in place)
// then relaunches the restored binary. The leftover <target>.tmp (the
// previously-installed release) is removed at the next startup by
// cleanupArtifacts(). Returns false if any step fails.
bool restore(const QString& targetPath, QString* err);

// Remove leftover files of an interrupted update/restore (<exe>.part,
// <exe>.tmp). Called once at startup.
void cleanupArtifacts();

} // namespace Updater
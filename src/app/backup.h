#pragma once

// ─────────────────────────────────────────────────────────────────────
// backup.h — database backup helpers.
//
// A backup is a single ZIP archive (see zipwriter.h) containing the
// current items.db, users.db and a SUMMARY.txt snapshot. It can be
// written to a local folder, attached to an e-mail, or PUT to a custom
// server endpoint. Online transports are OFF by default and toggled by
// the SuperAdmin in the Automation settings page.
// ─────────────────────────────────────────────────────────────────────

#include <QByteArray>
#include <QDateTime>
#include <QString>

namespace Backup {

// Pack current DBs (from the saved db/itemsPath & db/usersPath settings)
// plus a summary text file into one in-memory ZIP archive.
QByteArray buildBundle();

// Filename used for local backups and attachments: QMark-backup-<ts>.zip
QString archiveFileName(const QDateTime& when);

// Write a bundle into folder, prune old archives down to keepCount.
// Returns the full path of the newly created archive, or empty on failure.
QString createLocal(const QString& folder, int keepCount, QString* err);

// Upload a bundle to a custom server (HTTP PUT / WebDAV-style) with an
// optional bearer token. URL may contain a trailing file name or use a
// {filename} placeholder; otherwise the archive file name is appended.
bool uploadHttp(const QString& baseUrl, const QString& token,
                const QByteArray& bundle, const QString& fileName, QString* err);

} // namespace Backup
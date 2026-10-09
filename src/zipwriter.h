#pragma once

// ─────────────────────────────────────────────────────────────────────
// zipwriter.h — minimal ZIP archive writer (store method, no compression).
// Used to package DB backups as a single file for local folders, e-mail
// attachments and HTTP uploads. QtCore only.
// ─────────────────────────────────────────────────────────────────────

#include <QByteArray>
#include <QList>
#include <QPair>
#include <QString>

namespace Zip {

// CRCPP32/2 checksum (the one used by ZIP).
quint32 crc32(const QByteArray& data);

// Pack a list of {filename, content} entries into a ZIP archive.
// Fixed timestamps and store-only entries keep the builder deterministic.
QByteArray pack(const QList<QPair<QString, QByteArray>>& files);

} // namespace Zip
#pragma once

// ─────────────────────────────────────────────────────────────────────
// netutil.h — blocking HTTP helpers for background threads.
//
// QNetworkAccessManager is event-driven, so these helpers spin a local
// QEventLoop on the calling thread. They are meant to be run from worker
// threads (never from the GUI thread) and from Qt require QtNetwork.
// ─────────────────────────────────────────────────────────────────────

#include <QByteArray>
#include <QPair>
#include <QString>
#include <QUrl>

namespace Net {

using Headers = QList<QPair<QByteArray, QByteArray>>;

// Perform a GET, return the response body. Sets ok=false + err on failure
// (network error, timeout, or HTTP status >= 400).
QByteArray httpGet(const QUrl& url, const Headers& headers,
                   int timeoutMs, bool* ok, QString* err);

// Download url into destPath (binary-safe). Verifies size when expectedSize>=0.
bool httpDownload(const QUrl& url, const Headers& headers,
                  const QString& destPath, qint64 expectedSize,
                  int timeoutMs, QString* err);

// PUT body to url, optionally with a bearer token. ok=false on failure.
bool httpPut(const QUrl& url, const Headers& headers, const QByteArray& body,
             int timeoutMs, QString* err);

} // namespace Net
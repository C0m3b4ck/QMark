#include "netutil.h"
#include "version.h"

#include <QNetworkAccessManager>
#include <QNetworkReply>
#include <QNetworkRequest>
#include <QEventLoop>
#include <QTimer>
#include <QFile>
#include <QCoreApplication>

namespace Net {

namespace {
// Run a reply to completion with an overall timeout. Returns the reply
// (finished / error / timed-out) for the caller to inspect. The reply is
// scheduled for deletion by the caller when done.
void waitFinished(QNetworkReply* reply, int timeoutMs)
{
    QEventLoop loop;
    QTimer timer;
    timer.setSingleShot(true);
    QObject::connect(&timer, &QTimer::timeout, &loop, &QEventLoop::quit);
    QObject::connect(reply, &QNetworkReply::finished, &loop, &QEventLoop::quit);
    if (timeoutMs > 0) timer.start(timeoutMs);
    loop.exec();
    reply->disconnect(); // detach from the (stack) loop
}

QString describeReply(QNetworkReply* reply)
{
    const QNetworkReply::NetworkError e = reply->error();
    if (e == QNetworkReply::NoError) return QString();
    return reply->errorString() + QString(" (HTTP %1)")
        .arg(reply->attribute(QNetworkRequest::HttpStatusCodeAttribute).toInt());
}
} // namespace

QByteArray httpGet(const QUrl& url, const Headers& headers,
                   int timeoutMs, bool* ok, QString* err)
{
    if (ok) *ok = false;

    QNetworkAccessManager mgr;
    QNetworkRequest req(url);
    req.setTransferTimeout(timeoutMs > 0 ? timeoutMs : 30000);
    req.setHeader(QNetworkRequest::UserAgentHeader,
                  QStringLiteral("QMark/%1").arg(QStringLiteral(QMARK_VERSION_STRING)));
    for (const auto& h : headers) req.setRawHeader(h.first, h.second);

    QNetworkReply* reply = mgr.get(req);
    waitFinished(reply, timeoutMs);

    QByteArray body = reply->readAll();
    if (reply->error() != QNetworkReply::NoError) {
        if (err) *err = describeReply(reply);
        reply->deleteLater();
        return body;
    }
    if (ok) *ok = true;
    reply->deleteLater();
    return body;
}

bool httpDownload(const QUrl& url, const Headers& headers,
                  const QString& destPath, qint64 expectedSize,
                  int timeoutMs, QString* err)
{
    QNetworkAccessManager mgr;
    QNetworkRequest req(url);
    req.setTransferTimeout(timeoutMs > 0 ? timeoutMs : 600000);
    req.setHeader(QNetworkRequest::UserAgentHeader,
                  QStringLiteral("QMark/%1").arg(QStringLiteral(QMARK_VERSION_STRING)));
    for (const auto& h : headers) req.setRawHeader(h.first, h.second);

    QNetworkReply* reply = mgr.get(req);

    QFile file(destPath);
    if (!file.open(QIODevice::WriteOnly | QIODevice::Truncate)) {
        if (err) *err = QStringLiteral("Cannot write %1").arg(destPath);
        reply->deleteLater();
        return false;
    }

    // Not using readyRead streaming + local loop because readAll() after
    // finish is simplest; downloads here are at most a few dozen MB.
    waitFinished(reply, timeoutMs);

    if (reply->error() != QNetworkReply::NoError) {
        file.close();
        if (err) *err = describeReply(reply);
        reply->deleteLater();
        QFile::remove(destPath);
        return false;
    }

    const QByteArray data = reply->readAll();
    reply->deleteLater();
    file.write(data);
    file.close();

    if (expectedSize >= 0 && data.size() != expectedSize) {
        if (err) *err = QStringLiteral("Size mismatch: got %1, expected %2 bytes")
            .arg(data.size()).arg(expectedSize);
        QFile::remove(destPath);
        return false;
    }
    return true;
}

bool httpPut(const QUrl& url, const Headers& headers, const QByteArray& body,
             int timeoutMs, QString* err)
{
    QNetworkAccessManager mgr;
    QNetworkRequest req(url);
    req.setTransferTimeout(timeoutMs > 0 ? timeoutMs : 120000);
    req.setHeader(QNetworkRequest::UserAgentHeader,
                  QStringLiteral("QMark/%1").arg(QStringLiteral(QMARK_VERSION_STRING)));
    req.setHeader(QNetworkRequest::ContentTypeHeader, QStringLiteral("application/octet-stream"));
    for (const auto& h : headers) req.setRawHeader(h.first, h.second);

    QNetworkReply* reply = mgr.put(req, body);
    waitFinished(reply, timeoutMs);

    if (reply->error() != QNetworkReply::NoError) {
        if (err) *err = describeReply(reply);
        reply->deleteLater();
        return false;
    }
    reply->deleteLater();
    return true;
}

} // namespace Net
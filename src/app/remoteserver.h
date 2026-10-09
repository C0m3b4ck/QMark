#pragma once

// ─────────────────────────────────────────────────────────────────────
// remoteserver.h — built-in LAN dashboard for remote SuperAdmin access.
//
// A minimal HTTP/1.1 server (plaintext, token-protected) that serves a
// read-only statistics page and a JSON API. It is intended for trusted
// local networks only; the token is the only gate, so use a long random
// one (generated automatically). Failed attempts are rate-limited per IP.
//
//   GET /health                 → { "status": "ok" }
//   GET /api/stats              → full statistics JSON
//   GET /api/daily              → persisted per-day snapshots JSON
//   GET /                       → human-readable dashboard HTML
//
// Auth:  Authorization: Bearer <token>, or ?token=<token> in the query.
// ─────────────────────────────────────────────────────────────────────

#include <QObject>
#include <QTcpServer>
#include <QTimer>
#include <QHash>

#include "dataaccess.h"

class RemoteServer : public QObject
{
    Q_OBJECT

public:
    explicit RemoteServer(DataAccess::IDataAccess& db, QObject* parent = nullptr);

    bool start(quint16 port, const QString& token, QString* err);
    void stop();
    bool isRunning() const { return m_running; }
    quint16 effectivePort() const;

signals:
    void logMessage(const QString& line);

private slots:
    void onNewConnection();
    void onReadyRead();
    void sweepIdleClients();

private:
    struct Client {
        QByteArray buffer;
        QTcpSocket* socket = nullptr;
        qint64 lastActivityMs = 0;   // for the idle-connection timeout
    };

    void handleRequest(Client& c);

    // JSON / HTML builders (computed on demand from the database).
    QByteArray statsJson() const;
    QByteArray dailyJson() const;
    QByteArray healthJson() const;
    QByteArray dashboardHtml() const;

    QByteArray response(int status, const char* contentType,
                        const QByteArray& body) const;
    bool authorized(const QString& authHeader, const QUrl& url) const;
    void noteFailedAuth(const QString& peer);
    bool isLockedOut(const QString& peer) const;
    static bool constantTimeEquals(const QByteArray& a, const QByteArray& b);

    DataAccess::IDataAccess& m_db;
    QTcpServer m_server;
    QByteArray m_token;
    QHash<QTcpSocket*, Client*> m_clients;      // per-connection buffer state
    QHash<QString, int> m_failCount;            // peer → consecutive failures
    QHash<QString, qint64> m_lockUntil;         // peer → unlock epoch ms
    bool m_running = false;

    // ── Anti-DoS limits ────────────────────────────────────────────
    // A slow/idle client (e.g. "slowloris": open a socket, send a partial
    // request, never finish) must not pin a connection forever, and the
    // number of simultaneous connections is capped.
    QTimer m_sweepTimer;
    static constexpr int kMaxClients = 64;
    static constexpr qint64 kIdleTimeoutMs = 30 * 1000;
};
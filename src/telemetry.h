#ifndef TELEMETRY_H
#define TELEMETRY_H

#include <QString>
#include <QFile>
#include <QTextStream>
#include <QDateTime>
#include <QDir>
#include <SQLiteCpp/Database.h>
#include <SQLiteCpp/Statement.h>
#include <memory>
#include <mutex>
#include <string>
#include <vector>

// ── Telemetry entry ────────────────────────────────────────────────
struct TelemetryEntry {
    QString timestamp;
    QString tag;       // INFO, CLICK, WARN, ERROR, SALE, etc.
    QString message;
    QString username;  // operating user ("" when none logged in)
    QString role;
};

// ── Telemetry writer: SQLite DB (no CSV files) ─────────────────────
class TelemetryStore {
public:
    TelemetryStore() = default;

    // Open the SQLite sink. Call once at startup.
    void open(const QString& dbPath) {
        std::lock_guard<std::mutex> lock(m_mutex);
        openDb(dbPath);
    }

    // Append a single entry to the sink (with the current user context).
    // Writes are buffered in memory and persisted by flush(); this keeps
    // per-keystroke/per-click recording cheap even during heavy use.
    void write(const QString& tag, const QString& message) {
        TelemetryEntry entry;
        entry.timestamp = QDateTime::currentDateTime().toString("yyyy-MM-dd hh:mm:ss.zzz");
        entry.tag = tag;
        entry.message = message;
        std::lock_guard<std::mutex> lock(m_mutex);
        entry.username = m_username;
        entry.role = m_role;
        m_pending.push_back(entry);
    }

    // Persist all buffered entries in one transaction.
    void flush() {
        std::lock_guard<std::mutex> lock(m_mutex);
        flushLocked();
    }

    // Remember who is operating so every entry carries their username
    // and role automatically.
    void setUser(const QString& username, const QString& role) {
        std::lock_guard<std::mutex> lock(m_mutex);
        m_username = username;
        m_role = role;
    }

    void clearUser() {
        std::lock_guard<std::mutex> lock(m_mutex);
        m_username.clear();
        m_role.clear();
    }

    // Convenience wrappers
    void logInfo(const QString& msg)    { write("INFO", msg); }
    void logClick(const QString& btn)   { write("CLICK", btn); }
    void logWarn(const QString& msg)    { write("WARN", msg); }
    void logError(const QString& msg)   { write("ERROR", msg); }
    void logSale(const QString& msg)    { write("SALE", msg); }

    void close() {
        std::lock_guard<std::mutex> lock(m_mutex);
        flushLocked();
        m_db.reset();
    }

    ~TelemetryStore() { close(); }

private:
    // Persist m_pending inside one transaction. Caller holds m_mutex.
    void flushLocked() {
        if (m_pending.empty() || !m_db) return;
        try {
            m_db->exec("BEGIN");
            SQLite::Statement stmt(*m_db,
                "INSERT INTO telemetry (timestamp, tag, message, user, role) "
                "VALUES (?, ?, ?, ?, ?)");
            for (const TelemetryEntry& e : m_pending) {
                stmt.bind(1, e.timestamp.toStdString());
                stmt.bind(2, e.tag.toStdString());
                stmt.bind(3, e.message.toStdString());
                stmt.bind(4, e.username.toStdString());
                stmt.bind(5, e.role.toStdString());
                stmt.exec();
                stmt.reset();
            }
            m_db->exec("COMMIT");
        } catch (const std::exception&) {
            // Swallow — telemetry must never crash the app
            try { m_db->exec("ROLLBACK"); } catch (const std::exception&) {}
        }
        m_pending.clear();
    }

    void openDb(const QString& path) {
        try {
            m_db = std::make_unique<SQLite::Database>(path.toStdString(),
                SQLite::OPEN_READWRITE | SQLite::OPEN_CREATE);
            m_db->exec(
                "CREATE TABLE IF NOT EXISTS telemetry ("
                "id INTEGER PRIMARY KEY AUTOINCREMENT,"
                "timestamp TEXT NOT NULL,"
                "tag TEXT NOT NULL,"
                "message TEXT NOT NULL,"
                "user TEXT,"
                "role TEXT"
                ");"
            );
            m_db->exec("CREATE INDEX IF NOT EXISTS idx_telemetry_tag ON telemetry(tag);");
            m_db->exec("CREATE INDEX IF NOT EXISTS idx_telemetry_ts ON telemetry(timestamp);");
        } catch (const std::exception&) {
            m_db.reset();
        }
    }

    std::unique_ptr<SQLite::Database> m_db;
    std::vector<TelemetryEntry>       m_pending;
    std::mutex                        m_mutex;
    QString                           m_username;
    QString                           m_role;
};

// ── Global singleton accessor ───────────────────────────────────────
inline TelemetryStore& telemetry() {
    static TelemetryStore store;
    return store;
}

#endif // TELEMETRY_H

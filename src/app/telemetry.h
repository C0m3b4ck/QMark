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
    void write(const QString& tag, const QString& message) {
        std::lock_guard<std::mutex> lock(m_mutex);

        TelemetryEntry entry;
        entry.timestamp = QDateTime::currentDateTime().toString("yyyy-MM-dd hh:mm:ss.zzz");
        entry.tag = tag;
        entry.message = message;
        entry.username = m_username;
        entry.role = m_role;

        if (m_db) {
            try {
                SQLite::Statement stmt(*m_db,
                    "INSERT INTO telemetry (timestamp, tag, message, user, role) "
                    "VALUES (?, ?, ?, ?, ?)");
                stmt.bind(1, entry.timestamp.toStdString());
                stmt.bind(2, entry.tag.toStdString());
                stmt.bind(3, entry.message.toStdString());
                stmt.bind(4, entry.username.toStdString());
                stmt.bind(5, entry.role.toStdString());
                stmt.exec();
            } catch (const std::exception&) {
                // Swallow — telemetry must never crash the app
            }
        }
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
        m_db.reset();
    }

    ~TelemetryStore() { close(); }

private:
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

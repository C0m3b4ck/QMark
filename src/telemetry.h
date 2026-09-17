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

    QString toCsvLine() const {
        return timestamp + "," + tag + "," + message;
    }

    static QString csvHeader() {
        return "Timestamp,Tag,Message";
    }
};

// ── Dual telemetry writer: CSV file + SQLite DB ────────────────────
class TelemetryStore {
public:
    TelemetryStore() = default;

    // Open both sinks. Call once at startup.
    void open(const QString& csvPath, const QString& dbPath) {
        std::lock_guard<std::mutex> lock(m_mutex);
        openCsv(csvPath);
        openDb(dbPath);
    }

    // Append a single entry to both sinks.
    void write(const QString& tag, const QString& message) {
        std::lock_guard<std::mutex> lock(m_mutex);

        TelemetryEntry entry;
        entry.timestamp = QDateTime::currentDateTime().toString("yyyy-MM-dd hh:mm:ss.zzz");
        entry.tag = tag;
        entry.message = message;

        // CSV
        if (m_csvFile && m_csvFile->isOpen() && m_csvStream) {
            *m_csvStream << entry.toCsvLine() << "\n";
            m_csvStream->flush();
        }

        // SQLite
        if (m_db) {
            try {
                SQLite::Statement stmt(*m_db,
                    "INSERT INTO telemetry (timestamp, tag, message) VALUES (?, ?, ?)");
                stmt.bind(1, entry.timestamp.toStdString());
                stmt.bind(2, entry.tag.toStdString());
                stmt.bind(3, entry.message.toStdString());
                stmt.exec();
            } catch (const std::exception&) {
                // Swallow — telemetry must never crash the app
            }
        }
    }

    // Convenience wrappers
    void logInfo(const QString& msg)    { write("INFO", msg); }
    void logClick(const QString& btn)   { write("CLICK", btn); }
    void logWarn(const QString& msg)    { write("WARN", msg); }
    void logError(const QString& msg)   { write("ERROR", msg); }
    void logSale(const QString& msg)    { write("SALE", msg); }

    void close() {
        std::lock_guard<std::mutex> lock(m_mutex);
        if (m_csvStream) { m_csvStream->flush(); m_csvStream.reset(); }
        if (m_csvFile && m_csvFile->isOpen()) m_csvFile->close();
        m_csvFile.reset();
        m_db.reset();
    }

    ~TelemetryStore() { close(); }

private:
    void openCsv(const QString& path) {
        m_csvFile = std::make_unique<QFile>(path);
        if (m_csvFile->open(QIODevice::Append | QIODevice::Text)) {
            m_csvStream = std::make_unique<QTextStream>(m_csvFile.get());
            // Write header if file is empty
            if (m_csvFile->size() == 0) {
                *m_csvStream << TelemetryEntry::csvHeader() << "\n";
                m_csvStream->flush();
            }
        }
    }

    void openDb(const QString& path) {
        try {
            m_db = std::make_unique<SQLite::Database>(
                path.toStdString(),
                SQLite::OPEN_READWRITE | SQLite::OPEN_CREATE);
            m_db->exec(
                "CREATE TABLE IF NOT EXISTS telemetry ("
                "id INTEGER PRIMARY KEY AUTOINCREMENT,"
                "timestamp TEXT NOT NULL,"
                "tag TEXT NOT NULL,"
                "message TEXT NOT NULL"
                ");"
            );
            m_db->exec("CREATE INDEX IF NOT EXISTS idx_telemetry_tag ON telemetry(tag);");
            m_db->exec("CREATE INDEX IF NOT EXISTS idx_telemetry_ts ON telemetry(timestamp);");
        } catch (const std::exception&) {
            m_db.reset();
        }
    }

    std::unique_ptr<QFile>         m_csvFile;
    std::unique_ptr<QTextStream>   m_csvStream;
    std::unique_ptr<SQLite::Database> m_db;
    std::mutex                     m_mutex;
};

// ── Global singleton accessor ───────────────────────────────────────
inline TelemetryStore& telemetry() {
    static TelemetryStore store;
    return store;
}

#endif // TELEMETRY_H

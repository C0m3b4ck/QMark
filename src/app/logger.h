#ifndef LOGGER_H
#define LOGGER_H

#include <QDebug>
#include <QFile>
#include <QTextStream>
#include <QDateTime>
#include <QString>
#include <memory>
#include <mutex>

class AppLogger {
public:
    static AppLogger& instance() {
        static AppLogger inst;
        return inst;
    }

    void setTelemetryEnabled(bool enabled) {
        m_telemetryEnabled = enabled;
    }

    bool isTelemetryEnabled() const {
        return m_telemetryEnabled;
    }

    void setLogFile(const QString& path) {
        std::lock_guard<std::mutex> lock(m_mutex);
        if (m_logFile && m_logFile->isOpen()) {
            m_logFile->close();
        }
        m_logFile = std::make_unique<QFile>(path);
        if (m_logFile->open(QIODevice::Append | QIODevice::Text)) {
            m_stream = std::make_unique<QTextStream>(m_logFile.get());
        }
    }

    // Remember who is operating so telemetry lines carry the user.
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

    void log(const QString& tag, const QString& message) {
        QString timestamp = QDateTime::currentDateTime().toString("yyyy-MM-dd hh:mm:ss.zzz");
        QString user = m_username.isEmpty() ? "-" : m_username;
        QString role = m_role.isEmpty() ? "-" : m_role;
        QString entry = QString("[%1] [%2] [user: %3] [role: %4] %5")
            .arg(timestamp, tag, user, role, message);

        qDebug().noquote() << entry;

        if (m_telemetryEnabled) {
            std::lock_guard<std::mutex> lock(m_mutex);
            if (m_stream) {
                *m_stream << entry << "\n";
                m_stream->flush();
            }
        }
    }

    void logClick(const QString& buttonName) {
        log("CLICK", buttonName);
    }

    void close() {
        std::lock_guard<std::mutex> lock(m_mutex);
        if (m_stream) {
            m_stream->flush();
            m_stream.reset();
        }
        if (m_logFile && m_logFile->isOpen()) {
            m_logFile->close();
        }
        m_logFile.reset();
    }

private:
    AppLogger() = default;
    bool m_telemetryEnabled = false;
    std::unique_ptr<QFile> m_logFile;
    std::unique_ptr<QTextStream> m_stream;
    std::mutex m_mutex;
    QString m_username;
    QString m_role;
};

#define LOG_TAG(tag, msg) AppLogger::instance().log(tag, msg)
#define LOG_CLICK(button) AppLogger::instance().logClick(button)
#define LOG_INFO(msg) LOG_TAG("INFO", msg)
#define LOG_WARN(msg) LOG_TAG("WARN", msg)
#define LOG_ERROR(msg) LOG_TAG("ERROR", msg)

#endif // LOGGER_H

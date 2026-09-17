#ifndef WORKLOG_H
#define WORKLOG_H

#include <QString>
#include <QFile>
#include <QTextStream>
#include <QDateTime>
#include <QDir>
#include <vector>
#include <mutex>

struct WorklogEntry {
    enum class ActionType { Add, Edit, Remove, Sale };
    enum class EntityType { Item, Sale, Category, Shelf, User };

    ActionType action;
    EntityType entity;
    std::string entityId;
    std::string description;
    std::string timestamp;

    QString actionStr() const {
        switch (action) {
            case ActionType::Add:    return "ADD";
            case ActionType::Edit:   return "EDIT";
            case ActionType::Remove: return "REMOVE";
            case ActionType::Sale:   return "SALE";
        }
        return "UNKNOWN";
    }

    QString entityStr() const {
        switch (entity) {
            case EntityType::Item:     return "ITEM";
            case EntityType::Sale:     return "SALE";
            case EntityType::Category: return "CATEGORY";
            case EntityType::Shelf:    return "SHELF";
            case EntityType::User:     return "USER";
        }
        return "UNKNOWN";
    }

    QString toLogString() const {
        return QString("[%1] [%2] [%3] ID:%4 %5")
            .arg(QString::fromStdString(timestamp))
            .arg(actionStr())
            .arg(entityStr())
            .arg(QString::fromStdString(entityId))
            .arg(QString::fromStdString(description));
    }
};

class Worklog {
public:
    Worklog() = default;

    void setEnabled(bool enabled) { m_enabled = enabled; }
    bool isEnabled() const { return m_enabled; }

    void setSessionStart(const QDateTime& start) { m_sessionStart = start; }
    QDateTime getSessionStart() const { return m_sessionStart; }

    void setLogFile(const QString& path) {
        std::lock_guard<std::mutex> lock(m_mutex);
        if (m_file && m_file->isOpen()) {
            m_file->close();
        }
        m_file = std::make_unique<QFile>(path);
        m_sessionStart = QDateTime::currentDateTime();
        if (m_file->open(QIODevice::Append | QIODevice::Text)) {
            m_stream = std::make_unique<QTextStream>(m_file.get());
            *m_stream << "=== QMark Worklog Session Started: "
                      << m_sessionStart.toString("yyyy-MM-dd hh:mm:ss") << " ===\n";
            m_stream->flush();
        }
    }

    void logEntry(WorklogEntry::ActionType action, WorklogEntry::EntityType entity,
                  const std::string& entityId, const std::string& description) {
        std::lock_guard<std::mutex> lock(m_mutex);
        WorklogEntry entry;
        entry.action = action;
        entry.entity = entity;
        entry.entityId = entityId;
        entry.description = description;
        entry.timestamp = QDateTime::currentDateTime().toString("yyyy-MM-dd hh:mm:ss.zzz").toStdString();

        m_entries.push_back(entry);

        if (m_enabled && m_stream) {
            *m_stream << entry.toLogString() << "\n";
            m_stream->flush();
        }
    }

    const std::vector<WorklogEntry>& getEntries() const { return m_entries; }

    int getItemAddCount() const { return countBy(WorklogEntry::ActionType::Add, WorklogEntry::EntityType::Item); }
    int getItemEditCount() const { return countBy(WorklogEntry::ActionType::Edit, WorklogEntry::EntityType::Item); }
    int getItemRemoveCount() const { return countBy(WorklogEntry::ActionType::Remove, WorklogEntry::EntityType::Item); }

    int getSaleCount() const { return countBy(WorklogEntry::ActionType::Sale, WorklogEntry::EntityType::Sale); }

    int getCategoryAddCount() const { return countBy(WorklogEntry::ActionType::Add, WorklogEntry::EntityType::Category); }
    int getCategoryEditCount() const { return countBy(WorklogEntry::ActionType::Edit, WorklogEntry::EntityType::Category); }
    int getCategoryRemoveCount() const { return countBy(WorklogEntry::ActionType::Remove, WorklogEntry::EntityType::Category); }

    int getShelfAddCount() const { return countBy(WorklogEntry::ActionType::Add, WorklogEntry::EntityType::Shelf); }
    int getShelfEditCount() const { return countBy(WorklogEntry::ActionType::Edit, WorklogEntry::EntityType::Shelf); }
    int getShelfRemoveCount() const { return countBy(WorklogEntry::ActionType::Remove, WorklogEntry::EntityType::Shelf); }

    int getUserAddCount() const { return countBy(WorklogEntry::ActionType::Add, WorklogEntry::EntityType::User); }
    int getUserEditCount() const { return countBy(WorklogEntry::ActionType::Edit, WorklogEntry::EntityType::User); }
    int getUserRemoveCount() const { return countBy(WorklogEntry::ActionType::Remove, WorklogEntry::EntityType::User); }

    void close() {
        std::lock_guard<std::mutex> lock(m_mutex);
        if (m_stream) {
            *m_stream << "=== QMark Worklog Session Ended: "
                      << QDateTime::currentDateTime().toString("yyyy-MM-dd hh:mm:ss") << " ===\n";
            *m_stream << "\n";
            *m_stream << "Time start: " << m_sessionStart.toString("yyyy-MM-dd hh:mm:ss") << "\n";
            *m_stream << "Time end:   " << QDateTime::currentDateTime().toString("yyyy-MM-dd hh:mm:ss") << "\n";
            *m_stream << "Items added: " << getItemAddCount() << "\n";
            *m_stream << "Items edited: " << getItemEditCount() << "\n";
            *m_stream << "Items removed: " << getItemRemoveCount() << "\n";
            *m_stream << "Sales recorded: " << getSaleCount() << "\n";
            *m_stream << "Categories added: " << getCategoryAddCount() << "\n";
            *m_stream << "Categories edited: " << getCategoryEditCount() << "\n";
            *m_stream << "Categories removed: " << getCategoryRemoveCount() << "\n";
            *m_stream << "Shelves added: " << getShelfAddCount() << "\n";
            *m_stream << "Shelves edited: " << getShelfEditCount() << "\n";
            *m_stream << "Shelves removed: " << getShelfRemoveCount() << "\n";
            *m_stream << "Users added: " << getUserAddCount() << "\n";
            *m_stream << "Users edited: " << getUserEditCount() << "\n";
            *m_stream << "Users removed: " << getUserRemoveCount() << "\n";
            m_stream->flush();
            m_stream.reset();
        }
        if (m_file && m_file->isOpen()) {
            m_file->close();
        }
        m_file.reset();
    }

    static QString generateSessionFileName() {
        return "worklog_" + QDateTime::currentDateTime().toString("yyyyMMdd_hhmmss") + ".log";
    }

private:
    int countBy(WorklogEntry::ActionType action, WorklogEntry::EntityType entity) const {
        int count = 0;
        for (const auto& e : m_entries) {
            if (e.action == action && e.entity == entity) ++count;
        }
        return count;
    }

    bool m_enabled = false;
    std::vector<WorklogEntry> m_entries;
    std::unique_ptr<QFile> m_file;
    std::unique_ptr<QTextStream> m_stream;
    std::mutex m_mutex;
    QDateTime m_sessionStart = QDateTime::currentDateTime();
};

#endif // WORKLOG_H

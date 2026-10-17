#pragma once

// ─────────────────────────────────────────────────────────────────────
// assistantdialog.h — the optional "QMark Assistant" window.
//
// Two responsibilities:
//   1. On first use, download the fine-tuned model + local runtime from
//      the QMark release (behind a *double* confirmation) with progress.
//   2. Provide a simple chat box that asks the local model about QMark
//      features. Answers come from the on-device runtime; nothing is
//      sent to the internet.
//
// The class deliberately does not use Q_OBJECT: it only needs lambda
// connections to the download manager, which keeps the build scripts
// (which moc a fixed header list) unchanged.
// ─────────────────────────────────────────────────────────────────────

#include <QDialog>
#include <QString>
#include <QStringList>
#include <condition_variable>
#include <memory>
#include <mutex>
#include <thread>

class QLabel;
class QLineEdit;
class QPlainTextEdit;
class QProgressBar;
class QPushButton;
class QNetworkAccessManager;
class QNetworkReply;
class QFile;

namespace Assistant { class Runtime; }

class AssistantDialog : public QDialog
{
public:
    explicit AssistantDialog(QWidget* parent = nullptr);
    ~AssistantDialog() override;

private:
    void buildUi();
    void refreshStatus();
    void onDownloadClicked();
    void onDeleteClicked();
    void startDownloads();
    void downloadNext();
    void onDownloadProgress(qint64 received, qint64 total);
    void onDownloadFinished();
    void onAskClicked();
    void appendTurn(const QString& who, const QString& text);

    // Persistent worker thread that owns the local runtime. Creating and
    // using the QProcess on one dedicated thread keeps Qt's thread
    // affinity rules satisfied.
    void workerLoop();

    QLabel*         m_lblIntro = nullptr;
    QLabel*         m_lblModel = nullptr;
    QLabel*         m_lblRuntime = nullptr;
    QProgressBar*   m_progress = nullptr;
    QPushButton*    m_btnDownload = nullptr;
    QPushButton*    m_btnDelete = nullptr;
    QLabel*         m_lblStatus = nullptr;

    QPlainTextEdit* m_transcript = nullptr;
    QLineEdit*      m_input = nullptr;
    QPushButton*    m_btnAsk = nullptr;
    QPushButton*    m_btnClear = nullptr;

    QNetworkAccessManager* m_net = nullptr;
    QNetworkReply*         m_reply = nullptr;
    QFile*                 m_outFile = nullptr;   // streams the current download
    QStringList            m_queueUrls;
    QStringList            m_queueDests;
    bool                   m_verifySha = false;   // verify sha for the model
    int                    m_queueIndex = 0;

    // Worker-thread state (only touched by the worker or under m_mutex).
    std::unique_ptr<Assistant::Runtime> m_runtime;
    std::thread            m_worker;
    std::mutex             m_mutex;
    std::condition_variable m_cv;
    QString                m_pendingQuestion;
    bool                   m_hasPending = false;
    bool                   m_stopRuntime = false;
    bool                   m_quit = false;
    bool                   m_busy = false;
};

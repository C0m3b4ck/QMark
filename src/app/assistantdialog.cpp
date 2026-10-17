#include "assistantdialog.h"
#include "assistant.h"
#include "appsettings.h"
#include "translations.h"

#include <QCryptographicHash>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QFont>
#include <QGroupBox>
#include <QHBoxLayout>
#include <QLabel>
#include <QLineEdit>
#include <QMessageBox>
#include <QNetworkAccessManager>
#include <QNetworkReply>
#include <QNetworkRequest>
#include <QPlainTextEdit>
#include <QProgressBar>
#include <QPushButton>
#include <QTextCursor>
#include <QVBoxLayout>
#include <QMetaObject>

#include <chrono>

namespace {
QString humanSize(qint64 bytes)
{
    if (bytes >= 1024 * 1024)
        return QStringLiteral("%1 MB").arg(double(bytes) / (1024.0 * 1024.0), 0, 'f', 1);
    if (bytes >= 1024)
        return QStringLiteral("%1 KB").arg(double(bytes) / 1024.0, 0, 'f', 1);
    return QStringLiteral("%1 B").arg(bytes);
}
} // namespace

AssistantDialog::AssistantDialog(QWidget* parent)
    : QDialog(parent)
{
    setWindowTitle(Tr::trS("QMark Assistant"));
    resize(640, 560);
    buildUi();
    refreshStatus();

    // Start the dedicated runtime thread. It stays idle until a question
    // is asked and only then spawns the local model process.
    m_worker = std::thread([this] { workerLoop(); });
}

AssistantDialog::~AssistantDialog()
{
    // Abort any in-flight download so its lambdas stop firing.
    if (m_reply) {
        m_reply->disconnect();
        m_reply->abort();
        m_reply->deleteLater();
        m_reply = nullptr;
    }
    if (m_outFile) {
        m_outFile->close();
        delete m_outFile;
        m_outFile = nullptr;
    }

    {
        std::lock_guard<std::mutex> lock(m_mutex);
        m_quit = true;
        m_hasPending = false;
    }
    m_cv.notify_all();
    if (m_worker.joinable()) m_worker.join();
}

void AssistantDialog::buildUi()
{
    auto* root = new QVBoxLayout(this);

    m_lblIntro = new QLabel(this);
    m_lblIntro->setWordWrap(true);
    m_lblIntro->setText(Tr::trS(
        "QMark Assistant is a small, offline helper that answers questions about "
        "how to use QMark (for example \u201chow do I add an item?\u201d or "
        "\u201chow do I enable the scanner?\u201d). It runs entirely on this "
        "computer and knows nothing outside QMark."));
    root->addWidget(m_lblIntro);

    auto* files = new QGroupBox(Tr::trS("Assistant files"), this);
    auto* fl = new QVBoxLayout(files);
    m_lblModel = new QLabel(files);
    m_lblRuntime = new QLabel(files);
    fl->addWidget(m_lblModel);
    fl->addWidget(m_lblRuntime);
    m_progress = new QProgressBar(files);
    m_progress->setRange(0, 100);
    m_progress->setValue(0);
    m_progress->setVisible(false);
    fl->addWidget(m_progress);
    auto* fbtn = new QHBoxLayout();
    m_btnDownload = new QPushButton(Tr::trS("Download assistant files\u2026"), files);
    m_btnDelete = new QPushButton(Tr::trS("Delete downloaded files"), files);
    fbtn->addWidget(m_btnDownload);
    fbtn->addWidget(m_btnDelete);
    fbtn->addStretch();
    fl->addLayout(fbtn);
    root->addWidget(files);

    auto* chat = new QGroupBox(Tr::trS("Ask about QMark"), this);
    auto* cl = new QVBoxLayout(chat);
    m_transcript = new QPlainTextEdit(chat);
    m_transcript->setReadOnly(true);
    m_transcript->setPlaceholderText(Tr::trS("Answers will appear here."));
    cl->addWidget(m_transcript);
    auto* row = new QHBoxLayout();
    m_input = new QLineEdit(chat);
    m_input->setPlaceholderText(Tr::trS("Type your question about QMark\u2026"));
    m_btnAsk = new QPushButton(Tr::trS("Ask"), chat);
    m_btnClear = new QPushButton(Tr::trS("Clear"), chat);
    row->addWidget(m_input, 1);
    row->addWidget(m_btnAsk);
    row->addWidget(m_btnClear);
    cl->addLayout(row);
    root->addWidget(chat, 1);

    m_lblStatus = new QLabel(this);
    m_lblStatus->setWordWrap(true);
    root->addWidget(m_lblStatus);

    auto* bottom = new QHBoxLayout();
    bottom->addStretch();
    auto* btnClose = new QPushButton(Tr::trS("Close"), this);
    bottom->addWidget(btnClose);
    root->addLayout(bottom);

    connect(m_btnDownload, &QPushButton::clicked, this, [this] { onDownloadClicked(); });
    connect(m_btnDelete,   &QPushButton::clicked, this, [this] { onDeleteClicked(); });
    connect(m_btnAsk,      &QPushButton::clicked, this, [this] { onAskClicked(); });
    connect(m_btnClear,    &QPushButton::clicked, this, [this] {
        if (m_transcript) m_transcript->clear();
    });
    connect(m_input, &QLineEdit::returnPressed, this, [this] { onAskClicked(); });
    connect(btnClose, &QPushButton::clicked, this, [this] { close(); });
}

void AssistantDialog::refreshStatus()
{
    const bool m = Assistant::modelReady();
    const bool r = Assistant::runtimeReady();

    m_lblModel->setText(QStringLiteral("%1: %2")
        .arg(Tr::trS("Model"),
             m ? Tr::trS("installed") : Tr::trS("not downloaded")));
    m_lblRuntime->setText(QStringLiteral("%1: %2")
        .arg(Tr::trS("Runtime"),
             r ? Tr::trS("installed") : Tr::trS("not downloaded")));

    m_btnDownload->setEnabled(!(m && r));
    m_btnDelete->setEnabled(m || r);
    m_input->setEnabled(m && r);
    m_btnAsk->setEnabled(m && r && !m_busy);

    if (m && r)
        m_lblStatus->setText(Tr::trS("The assistant is ready. Ask a question below."));
    else
        m_lblStatus->setText(Tr::trS(
            "The assistant needs a one-time download (about 380 MB) before it can answer."));
}

void AssistantDialog::onDownloadClicked()
{
    const auto a = QMessageBox::question(
        this, Tr::trS("Download assistant"),
        Tr::trS("QMark Assistant needs two files, downloaded once from the QMark "
                "project page:\n\n"
                "\u2022 the fine-tuned model (about 380 MB)\n"
                "\u2022 the local CPU runtime (a few MB)\n\n"
                "They are stored in an \u201cassistant\u201d folder next to QMark. "
                "Nothing about your shop is ever sent anywhere.\n\nDownload now?"),
        QMessageBox::Yes | QMessageBox::No, QMessageBox::No);
    if (a != QMessageBox::Yes) return;

    const auto b = QMessageBox::warning(
        this, Tr::trS("Please confirm again"),
        Tr::trS("You are about to download about 380 MB. On a slow connection this "
                "can take several minutes.\n\nPress Download to start, or Cancel to "
                "abort."),
        QMessageBox::Yes | QMessageBox::Cancel, QMessageBox::Cancel);
    if (b != QMessageBox::Yes) return;

    startDownloads();
}

void AssistantDialog::startDownloads()
{
    if (!m_net) m_net = new QNetworkAccessManager(this);

    QDir().mkpath(Assistant::dataDir());

    m_queueUrls.clear();
    m_queueDests.clear();

    if (!Assistant::modelReady()) {
        m_queueUrls << Assistant::modelUrl();
        m_queueDests << Assistant::modelPath();
    }
    if (!Assistant::runtimeReady()) {
        m_queueUrls << Assistant::runtimeUrl();
        m_queueDests << Assistant::runtimePath();
    }
    if (m_queueUrls.isEmpty()) { refreshStatus(); return; }

    m_queueIndex = 0;
    m_btnDownload->setEnabled(false);
    m_btnDelete->setEnabled(false);
    m_progress->setVisible(true);
    m_progress->setValue(0);
    m_lblStatus->setText(Tr::trS("Downloading\u2026"));
    downloadNext();
}

void AssistantDialog::downloadNext()
{
    if (m_queueIndex >= m_queueUrls.size()) {
        m_progress->setVisible(false);
        AppSettings::settings().setValue(AppSettings::keyAssistantInstalled(), true);
        refreshStatus();
        return;
    }

    const QUrl url(m_queueUrls.at(m_queueIndex));
    const QString dest = m_queueDests.at(m_queueIndex);
    m_verifySha = dest.endsWith(Assistant::modelAssetName());

    // Refuse anything that is not HTTPS.
    if (url.scheme() != QLatin1String("https")) {
        QMessageBox::warning(this, Tr::trS("Download assistant"),
                             Tr::trS("Refusing to download over an insecure connection."));
        m_progress->setVisible(false);
        refreshStatus();
        return;
    }

    const QString tmp = dest + QStringLiteral(".part");
    QFile::remove(tmp);

    // Stream straight to disk: the model is ~380 MB, so buffering it in
    // memory would be wasteful on a small machine.
    m_outFile = new QFile(tmp);
    if (!m_outFile->open(QIODevice::WriteOnly)) {
        delete m_outFile;
        m_outFile = nullptr;
        m_progress->setVisible(false);
        QMessageBox::warning(this, Tr::trS("Download assistant"),
                             Tr::trS("Could not save the downloaded file."));
        refreshStatus();
        return;
    }

    const bool isModel = dest.endsWith(Assistant::modelAssetName());
    m_lblStatus->setText(isModel
        ? Tr::trS("Downloading %1 (%2)\u2026")
              .arg(QFileInfo(dest).fileName(), humanSize(Assistant::modelSize()))
        : Tr::trS("Downloading %1\u2026").arg(QFileInfo(dest).fileName()));

    QNetworkRequest req(url);
    req.setAttribute(QNetworkRequest::RedirectPolicyAttribute,
                     QNetworkRequest::NoLessSafeRedirectPolicy);
    req.setHeader(QNetworkRequest::UserAgentHeader, QStringLiteral("QMark"));

    m_reply = m_net->get(req);
    m_reply->setProperty("dest", dest);
    m_reply->setProperty("tmp", tmp);

    connect(m_reply, &QNetworkReply::readyRead, this, [this] {
        if (m_reply && m_outFile) m_outFile->write(m_reply->readAll());
    });
    connect(m_reply, &QNetworkReply::downloadProgress, this,
            [this](qint64 r, qint64 t) { onDownloadProgress(r, t); });
    connect(m_reply, &QNetworkReply::finished, this,
            [this] { onDownloadFinished(); });
}

void AssistantDialog::onDownloadProgress(qint64 received, qint64 total)
{
    if (total > 0) {
        m_progress->setRange(0, 100);
        m_progress->setValue(int(received * 100 / total));
    } else {
        m_progress->setRange(0, 0);   // busy
    }
}

void AssistantDialog::onDownloadFinished()
{
    QNetworkReply* reply = m_reply;
    m_reply = nullptr;
    if (!reply) return;

    const QString dest = reply->property("dest").toString();
    const QString tmp  = reply->property("tmp").toString();
    const bool verify  = m_verifySha;

    // Flush the final bytes and close our streaming file.
    if (m_outFile) {
        m_outFile->write(reply->readAll());
        m_outFile->close();
        delete m_outFile;
        m_outFile = nullptr;
    }

    if (reply->error() != QNetworkReply::NoError) {
        const QString e = reply->errorString();
        reply->deleteLater();
        QFile::remove(tmp);
        m_progress->setVisible(false);
        QMessageBox::warning(this, Tr::trS("Download assistant"),
                             Tr::trS("The download failed: %1").arg(e));
        refreshStatus();
        return;
    }
    reply->deleteLater();

    // Optional integrity check for the model.
    if (verify) {
        QFile f(tmp);
        if (f.open(QIODevice::ReadOnly)) {
            QCryptographicHash h(QCryptographicHash::Sha256);
            if (h.addData(&f)) {
                const QString got = QString::fromLatin1(h.result().toHex());
                if (got != Assistant::modelSha256()) {
                    f.close();
                    QFile::remove(tmp);
                    m_progress->setVisible(false);
                    QMessageBox::warning(
                        this, Tr::trS("Download assistant"),
                        Tr::trS("The downloaded model failed its integrity check "
                                "and was discarded. Please try again."));
                    refreshStatus();
                    return;
                }
            }
        }
    }

    QFile::remove(dest);
    if (!QFile::rename(tmp, dest)) {
        QFile::remove(tmp);
        m_progress->setVisible(false);
        QMessageBox::warning(this, Tr::trS("Download assistant"),
                             Tr::trS("Could not finalise the downloaded file."));
        refreshStatus();
        return;
    }
#ifdef Q_OS_UNIX
    QFile::setPermissions(dest, QFile::ReadOwner | QFile::WriteOwner | QFile::ExeOwner
                                | QFile::ReadGroup | QFile::ExeGroup
                                | QFile::ReadOther | QFile::ExeOther);
#endif

    ++m_queueIndex;
    downloadNext();
}

void AssistantDialog::onDeleteClicked()
{
    const auto a = QMessageBox::question(
        this, Tr::trS("Delete assistant files"),
        Tr::trS("Delete the downloaded model and runtime? You can download them "
                "again later."),
        QMessageBox::Yes | QMessageBox::No, QMessageBox::No);
    if (a != QMessageBox::Yes) return;

    // Ask the worker thread to stop the local runtime before removing the
    // files (the process keeps the executable locked on Windows), and wait
    // briefly for it to confirm.
    {
        std::unique_lock<std::mutex> lock(m_mutex);
        m_stopRuntime = true;
        m_hasPending = false;
        m_cv.notify_all();
        m_cv.wait_for(lock, std::chrono::seconds(6), [this] { return !m_stopRuntime; });
    }

    QFile::remove(Assistant::modelPath());
    QFile::remove(Assistant::runtimePath());
    AppSettings::settings().setValue(AppSettings::keyAssistantInstalled(), false);
    refreshStatus();
}

void AssistantDialog::appendTurn(const QString& who, const QString& text)
{
    m_transcript->appendPlainText(QStringLiteral("%1: %2").arg(who, text));
    m_transcript->moveCursor(QTextCursor::End);
}

void AssistantDialog::onAskClicked()
{
    if (m_busy) return;
    const QString q = m_input->text().trimmed();
    if (q.isEmpty()) return;

    if (!Assistant::ready()) {
        QMessageBox::information(this, Tr::trS("QMark Assistant"),
                                 Tr::trS("Download the assistant files first."));
        return;
    }

    m_input->clear();
    appendTurn(Tr::trS("You"), q);

    m_busy = true;
    m_btnAsk->setEnabled(false);
    m_input->setEnabled(false);
    m_lblStatus->setText(Tr::trS("Thinking\u2026"));

    {
        std::lock_guard<std::mutex> lock(m_mutex);
        m_pendingQuestion = q;
        m_hasPending = true;
    }
    m_cv.notify_one();
}

void AssistantDialog::workerLoop()
{
    // Owns the runtime for its whole life: the QProcess is created and
    // used only on this thread.
    m_runtime.reset(new Assistant::Runtime());

    for (;;) {
        QString question;
        {
            std::unique_lock<std::mutex> lock(m_mutex);
            m_cv.wait(lock, [this] { return m_hasPending || m_stopRuntime || m_quit; });
            if (m_quit) break;
            if (m_stopRuntime) {
                // Stop the local model process, then tell the GUI thread.
                m_runtime->stop();
                m_stopRuntime = false;
                m_cv.notify_all();
                continue;
            }
            question = m_pendingQuestion;
            m_hasPending = false;
        }

        QString err;
        if (!m_runtime->isRunning() && !m_runtime->start(&err)) {
            // query() below reports the failure if the runtime never came up.
        }
        QString answer = m_runtime->query(question, &err);
        if (answer.isEmpty()) {
            if (err.isEmpty()) err = Tr::trS("no answer");
            answer = Tr::trS("(The assistant could not answer: %1)").arg(err);
        }

        QMetaObject::invokeMethod(this, [this, answer]() {
            appendTurn(Tr::trS("Assistant"), answer);
            m_busy = false;
            m_input->setEnabled(true);
            m_btnAsk->setEnabled(true);
            m_input->setFocus();
            refreshStatus();
        }, Qt::QueuedConnection);
    }

    // Destroy the runtime on this thread (correct thread affinity).
    m_runtime.reset();
}

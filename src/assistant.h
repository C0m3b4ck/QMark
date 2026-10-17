#pragma once

// ─────────────────────────────────────────────────────────────────────
// assistant.h — optional on-device help assistant ("QMark Assistant").
//
// A tiny (0.5B, Q4_K_M) language model fine-tuned exclusively on QMark's
// own features lives on the GitHub release as two assets:
//   * the GGUF model  (QMark-Assistant-Qwen2.5-0.5B-Instruct.Q4_K_M.gguf)
//   * the CPU runtime (a statically linked llama.cpp `llama-server`)
//
// Both are downloaded on demand (behind a double confirmation, see
// AssistantDialog) into <appdir>/assistant and then driven locally: the
// runtime is started on a private localhost port and the model is asked
// one question at a time over HTTP. Nothing ever leaves the machine, and
// the model answers *only* QMark questions (see systemPrompt()).
// ─────────────────────────────────────────────────────────────────────

#include <QObject>
#include <QString>

class QProcess;

namespace Assistant {

// ── Release assets ─────────────────────────────────────────────────
QString modelAssetName();        // GGUF file name on the release
QString runtimeAssetName();      // llama-server binary name on the release
QString modelSha256();           // expected hex digest of the model
qint64  modelSize();             // expected byte size (progress / check)
QString modelUrl();              // direct download URL (current release)
QString runtimeUrl();

// ── On-disk layout ─────────────────────────────────────────────────
QString dataDir();               // <appdir>/assistant (override via settings)
QString modelPath();             // dataDir()/modelAssetName()
QString runtimePath();           // dataDir()/llama-server[.exe]
QString systemPromptPath();      // dataDir()/system_prompt.txt

bool modelReady();               // file exists and is non-empty
bool runtimeReady();             // file exists and is non-empty
bool ready();                    // modelReady() && runtimeReady()

// The exact system prompt the model was fine-tuned with. Loaded from
// systemPromptPath() when present, otherwise a byte-identical embedded
// copy is returned.
QString systemPrompt();

// ── Local runtime (llama-server) ───────────────────────────────────
// This class is created and used from a single dedicated worker thread
// (see AssistantDialog): start() owns a QProcess and query() performs the
// blocking HTTP request, both on that same thread.
class Runtime
{
public:
    Runtime() = default;
    ~Runtime();

    Runtime(const Runtime&) = delete;
    Runtime& operator=(const Runtime&) = delete;

    bool start(QString* err);                 // launch + wait until ready
    void stop();
    bool isRunning() const;

    // Blocking single-turn query. Runs an HTTP request and returns the
    // assistant reply (empty + err on failure).
    QString query(const QString& question, QString* err);

private:
    QProcess* m_proc = nullptr;
    int m_port = 0;
};

} // namespace Assistant

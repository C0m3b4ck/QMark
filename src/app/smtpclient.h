#pragma once

// ─────────────────────────────────────────────────────────────────────
// smtpclient.h — minimal SMTP sender over Qt sockets.
//
// Supports implicit TLS (465), STARTTLS (587) and plaintext (25), plus
// AUTH LOGIN / AUTH PLAIN. Single-shot: blocking call, meant to run on a
// background thread. Attachments are base64-encoded inline (no external
// encoding libraries needed).
// ─────────────────────────────────────────────────────────────────────

#include <QByteArray>
#include <QList>
#include <QPair>
#include <QString>
#include <QStringList>

struct SmtpSettings {
    QString host;
    int port = 587;
    int security = 0;          // 0 = STARTTLS, 1 = implicit TLS, 2 = plaintext
    QString username;          // may be empty (no auth)
    QString password;
    QString from;              // sender e-mail address
    QString fromName;          // display name (may be empty)
};

struct SmtpMessage {
    QString subject;
    QString bodyPlain;
    QStringList recipients;      // destination e-mail addresses
    // Optional attachments: {filename, raw bytes}.
    QList<QPair<QString, QByteArray>> attachments;
};

// Sends the message; returns true on success, false otherwise (err set).
// Blocking — call from a worker thread, never the GUI thread.
bool smtpSend(const SmtpSettings& settings, const SmtpMessage& message,
              int timeoutMs, QString* err);
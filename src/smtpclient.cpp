#include "smtpclient.h"

#include <QSslSocket>
#include <QDateTime>

// ── SMTP response reading helpers ───────────────────────────────────

namespace {

// Read one SMTP reply. Multi-line replies arrive as:
//   250-PIPELINING\r\n
//   250-SIZE 35882577\r\n
//   250 OK\r\n
// A terminal line has a space after the 3-digit code. Returns the code.
int readReply(QSslSocket& sock, int timeoutMs)
{
    forever {
        if (!sock.waitForReadyRead(timeoutMs)) {
            return -1;                       // timeout or connection closed
        }
        while (sock.canReadLine()) {
            QByteArray line = sock.readLine();
            if (line.size() < 3) continue;
            bool okNum = false;
            const int code = line.left(3).toInt(&okNum);
            if (!okNum) continue;
            // "250-..." continues; "250 ..." (or bare "250\r\n") ends it.
            if (line.size() < 4 || line.at(3) == ' ') {
                if (line.size() == 5 && line.mid(3, 2) == "\r\n") return code;
                if (line.size() >= 4 && line.at(3) == ' ') return code;
            } else if (line.size() >= 4 && line.at(3) == '-') {
                continue;                    // more lines coming
            }
        }
    }
}

// Send a command (CRLF-terminated) and wait for its reply.
int command(QSslSocket& sock, const QByteArray& cmd, int timeoutMs)
{
    if (sock.write(cmd + "\r\n") == -1) return -1;
    sock.flush();
    // flush() already hands the bytes to the OS. waitForBytesWritten() can
    // return false when nothing is left buffered (no payload was written
    // during the wait), so it must not be treated as a send failure — only
    // wait while data is still pending, then read the reply.
    if (sock.bytesToWrite() > 0)
        sock.waitForBytesWritten(timeoutMs);
    return readReply(sock, timeoutMs);
}

QByteArray fold(const QString& s)
{
    // Minimal header folding: strip CR/LF so headers cannot be hijacked.
    return s.toUtf8().replace('\r', ' ').replace('\n', ' ');
}

QByteArray fromHeader(const SmtpSettings& s)
{
    if (s.fromName.isEmpty()) return "<" + s.from.toUtf8() + ">";
    return fold(s.fromName) + " <" + s.from.toUtf8() + ">";
}

QByteArray toHeader(const SmtpMessage& m)
{
    QStringList parts = m.recipients;
    for (int i = 0; i < parts.size(); ++i) {
        parts[i] = "<" + parts[i] + ">";
    }
    return parts.join(", ").toUtf8();
}

QByteArray base64Encode(const QString& s)
{
    return s.toUtf8().toBase64();
}

// SMTP transparency ("dot-stuffing"): a message line that starts with '.'
// must be sent with the dot doubled, otherwise a body line of "." would be
// read by the server as the end of DATA and truncate the message.
QByteArray dotStuff(const QByteArray& in)
{
    QByteArray out;
    out.reserve(in.size() + 16);
    bool atLineStart = true;
    for (const char ch : in) {
        if (atLineStart && ch == '.') out.append('.');
        out.append(ch);
        atLineStart = (ch == '\n');
    }
    return out;
}

} // namespace

bool smtpSend(const SmtpSettings& settings, const SmtpMessage& message,
              int timeoutMs, QString* err)
{
    if (timeoutMs <= 0) timeoutMs = 30000;

    if (settings.host.isEmpty()) {
        if (err) *err = QStringLiteral("SMTP host is not configured.");
        return false;
    }
    if (settings.from.isEmpty()) {
        if (err) *err = QStringLiteral("Sender e-mail address is not configured.");
        return false;
    }
    if (message.recipients.isEmpty()) {
        if (err) *err = QStringLiteral("No recipients configured.");
        return false;
    }

    QSslSocket sock;
    sock.setProtocol(QSsl::TlsV1_2OrLater);

    // ── Connect (implicit TLS or plain) ───────────────────────────
    if (settings.security == 1) {            // implicit TLS (e.g. port 465)
        sock.connectToHostEncrypted(settings.host, quint16(settings.port));
        if (!sock.waitForEncrypted(timeoutMs)) {
            if (err) *err = QStringLiteral("TLS handshake failed: %1").arg(sock.errorString());
            return false;
        }
    } else {
        sock.connectToHost(settings.host, quint16(settings.port));
        if (!sock.waitForConnected(timeoutMs)) {
            if (err) *err = QStringLiteral("Connection failed: %1").arg(sock.errorString());
            return false;
        }
    }

    // ── Greeting ──────────────────────────────────────────────────
    int code = readReply(sock, timeoutMs);
    if (code != 220) {
        if (err) *err = QStringLiteral("Unexpected SMTP greeting (code %1).").arg(code);
        sock.close();
        return false;
    }

    // ── EHLO ──────────────────────────────────────────────────────
    code = command(sock, "EHLO qmark", timeoutMs);
    if (code != 250) {
        if (err) *err = QStringLiteral("EHLO rejected (code %1).").arg(code);
        sock.close();
        return false;
    }

    // ── STARTTLS upgrade ──────────────────────────────────────────
    if (settings.security == 0) {
        code = command(sock, "STARTTLS", timeoutMs);
        if (code == 220) {
            sock.startClientEncryption();
            if (sock.waitForEncrypted(timeoutMs)) {
                code = command(sock, "EHLO qmark", timeoutMs);
                if (code != 250) {
                    if (err) *err = QStringLiteral("EHLO (TLS) rejected (code %1).").arg(code);
                    sock.close();
                    return false;
                }
            } else {
                if (err) *err = QStringLiteral("TLS upgrade failed: %1").arg(sock.errorString());
                sock.close();
                return false;
            }
        } else {
            // STARTTLS was explicitly selected (security == 0): a server
            // that does not offer it (or an active MITM stripping it) must
            // not cause us to fall back to sending credentials in the clear.
            if (err) *err = QStringLiteral("STARTTLS is required but the server did not offer it (code %1).").arg(code);
            sock.close();
            return false;
        }
    }

    // ── Authentication (optional) ─────────────────────────────────
    if (!settings.username.isEmpty()) {
        code = command(sock, "AUTH LOGIN", timeoutMs);
        if (code == 334) {
            code = command(sock, base64Encode(settings.username), timeoutMs);
            if (code == 334) code = command(sock, base64Encode(settings.password), timeoutMs);
        } else if (code != 235) {
            // LOGIN unavailable → AUTH PLAIN.
            const QByteArray plain = QByteArray("\0", 1) + settings.username.toUtf8()
                                   + QByteArray("\0", 1) + settings.password.toUtf8();
            code = command(sock, "AUTH PLAIN " + plain.toBase64(), timeoutMs);
        }
        if (code != 235) {
            if (err) *err = QStringLiteral("SMTP authentication failed (code %1).").arg(code);
            sock.close();
            return false;
        }
    }

    // ── MAIL FROM ─────────────────────────────────────────────────
    code = command(sock, "MAIL FROM: <" + settings.from.toUtf8() + ">", timeoutMs);
    if (code != 250) {
        if (err) *err = QStringLiteral("MAIL FROM rejected (code %1).").arg(code);
        sock.close();
        return false;
    }

    // ── RCPT TO ───────────────────────────────────────────────────
    for (const QString& rcpt : message.recipients) {
        code = command(sock, "RCPT TO: <" + rcpt.toUtf8() + ">", timeoutMs);
        if (code != 250 && code != 251) {
            if (err) *err = QStringLiteral("RCPT TO rejected for %1 (code %2).").arg(rcpt).arg(code);
            sock.close();
            return false;
        }
    }

    // ── DATA ──────────────────────────────────────────────────────
    code = command(sock, "DATA", timeoutMs);
    if (code != 354) {
        if (err) *err = QStringLiteral("DATA rejected (code %1).").arg(code);
        sock.close();
        return false;
    }

    // Compose MIME message.
    const QByteArray boundary = "QMBOUNDARY0001";
    QByteArray data;

    data += "From: " + fromHeader(settings) + "\r\n";
    data += "To: " + toHeader(message) + "\r\n";
    data += "Subject: " + fold(message.subject) + "\r\n";
    data += "Date: " + QDateTime::currentDateTimeUtc().toString("ddd, dd MMM yyyy HH:mm:ss +0000").toUtf8() + "\r\n";
    data += "MIME-Version: 1.0\r\n";

    if (message.attachments.isEmpty()) {
        data += "Content-Type: text/plain; charset=UTF-8\r\n";
        data += "Content-Transfer-Encoding: 8bit\r\n";
        data += "\r\n";
        data += message.bodyPlain.toUtf8();
        data += "\r\n";
    } else {
        data += "Content-Type: multipart/mixed; boundary=\"" + boundary + "\"\r\n";
        data += "\r\n";
        data += "--" + boundary + "\r\n";
        data += "Content-Type: text/plain; charset=UTF-8\r\n";
        data += "Content-Transfer-Encoding: 8bit\r\n";
        data += "\r\n";
        data += message.bodyPlain.toUtf8();
        data += "\r\n";
        for (const auto& att : message.attachments) {
            data += "--" + boundary + "\r\n";
            data += "Content-Type: application/octet-stream; name=\"" + fold(att.first) + "\"\r\n";
            data += "Content-Transfer-Encoding: base64\r\n";
            data += "Content-Disposition: attachment; filename=\"" + fold(att.first) + "\"\r\n";
            data += "\r\n";
            const QByteArray b64 = att.second.toBase64();
            for (int i = 0; i < b64.size(); i += 76) {
                data += b64.mid(i, 76);
                data += "\r\n";
            }
            data += "\r\n";
        }
        data += "--" + boundary + "--\r\n";
    }

    qint64 written = 0;
    const QByteArray escaped = dotStuff(data);
    while (written < escaped.size()) {
        qint64 n = sock.write(escaped.constData() + written, int(escaped.size() - written));
        if (n <= 0) break;
        written += n;
    }
    sock.flush();
    sock.waitForBytesWritten(timeoutMs);

    if (sock.write(".\r\n") == -1) {
        if (err) *err = QStringLiteral("Failed to finalize message data.");
        sock.close();
        return false;
    }
    sock.flush();
    sock.waitForBytesWritten(timeoutMs);

    code = readReply(sock, timeoutMs);
    if (code != 250) {
        if (err) *err = QStringLiteral("Message not accepted (code %1).").arg(code);
        sock.close();
        return false;
    }

    command(sock, "QUIT", 5000);
    sock.close();
    return true;
}
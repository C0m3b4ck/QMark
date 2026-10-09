#include "remoteserver.h"

#include <QTcpSocket>
#include <QHostAddress>
#include <QUrl>
#include <QUrlQuery>
#include <QDateTime>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonValue>
#include <algorithm>
#include <unordered_map>

#include "statistics.h"
#include "domain.h"
#include "version.h"

namespace {

QString htmlEscape(const QString& s)
{
    QString out = s;
    out.replace('&', "&amp;").replace('<', "&lt;").replace('>', "&gt;");
    return out;
}

} // namespace

RemoteServer::RemoteServer(DataAccess::IDataAccess& db, QObject* parent)
    : QObject(parent)
    , m_db(db)
{
    connect(&m_server, &QTcpServer::newConnection,
            this, &RemoteServer::onNewConnection);

    // Periodic sweep closes connections that never finish their request.
    m_sweepTimer.setInterval(5000);
    connect(&m_sweepTimer, &QTimer::timeout, this, &RemoteServer::sweepIdleClients);
    m_sweepTimer.start();
}

bool RemoteServer::start(quint16 port, const QString& token, QString* err)
{
    if (m_running) stop();
    m_token = token.toUtf8();

    if (!m_server.listen(QHostAddress::Any, port)) {
        if (err) *err = QStringLiteral("Cannot listen on port %1: %2")
            .arg(port).arg(m_server.errorString());
        return false;
    }
    m_running = true;
    emit logMessage(QStringLiteral("Remote dashboard listening on port %1")
                        .arg(m_server.serverPort()));
    return true;
}

void RemoteServer::stop()
{
    for (auto* c : m_clients) {
        if (c && c->socket) {
            c->socket->disconnect(this);
            c->socket->deleteLater();
        }
        delete c;
    }
    m_clients.clear();
    m_failCount.clear();
    m_lockUntil.clear();
    m_server.close();
    m_running = false;
}

quint16 RemoteServer::effectivePort() const
{
    return m_server.serverPort();
}

void RemoteServer::onNewConnection()
{
    while (QTcpSocket* socket = m_server.nextPendingConnection()) {
        // Reject beyond the connection cap (defense against connection
        // floods / slowloris).
        if (m_clients.size() >= kMaxClients) {
            // Drain anything already buffered: closing a socket that still
            // has unread data turns the close into a TCP reset, which would
            // discard the 503 below.
            socket->readAll();
            socket->write(response(503, "text/plain", "Server busy.\r\n"));
            socket->flush();
            socket->disconnectFromHost();
            connect(socket, &QTcpSocket::disconnected, socket, &QObject::deleteLater);
            socket->deleteLater();
            continue;
        }
        auto* client = new Client;
        client->socket = socket;
        client->buffer.clear();
        client->lastActivityMs = QDateTime::currentMSecsSinceEpoch();
        m_clients.insert(socket, client);
        connect(socket, &QTcpSocket::readyRead, this, &RemoteServer::onReadyRead);
        connect(socket, &QTcpSocket::disconnected, this, [this, socket]() {
            Client* c = m_clients.take(socket);
            delete c;
            socket->deleteLater();
        });
    }
}

void RemoteServer::sweepIdleClients()
{
    if (m_clients.isEmpty()) return;
    const qint64 now = QDateTime::currentMSecsSinceEpoch();
    // Snapshot keys: closing a socket must not mutate the hash while we
    // iterate (cleanup happens in the disconnected() handler).
    const QList<QTcpSocket*> sockets = m_clients.keys();
    for (QTcpSocket* s : sockets) {
        Client* c = m_clients.value(s);
        if (!c) continue;
        if (now - c->lastActivityMs > kIdleTimeoutMs) {
            s->write(response(408, "text/plain", "Request timeout.\r\n"));
            s->disconnectFromHost();
        }
    }
}

void RemoteServer::onReadyRead()
{
    QTcpSocket* socket = qobject_cast<QTcpSocket*>(sender());
    if (!socket) return;
    Client* c = m_clients.value(socket);
    if (!c) { socket->disconnectFromHost(); return; }

    c->lastActivityMs = QDateTime::currentMSecsSinceEpoch();
    c->buffer += socket->readAll();

    const int headEnd = c->buffer.indexOf("\r\n\r\n");
    if (headEnd < 0) {
        if (c->buffer.size() > 65536) {          // oversized request
            socket->write(response(431, "text/plain", "Request too large.\r\n"));
            socket->disconnectFromHost();
        }
        return;
    }

    handleRequest(*c);
    socket->disconnectFromHost();
}

void RemoteServer::handleRequest(Client& c)
{
    QTcpSocket* socket = c.socket;
    const QString peer = socket->peerAddress().toString();

    if (isLockedOut(peer)) {
        socket->write(response(429, "text/plain", "Too many failed attempts.\r\n"));
        return;
    }

    // ── Parse request line ───────────────────────────────────────
    const QByteArray head = c.buffer.left(c.buffer.indexOf("\r\n\r\n"));
    const QList<QByteArray> lines = head.split('\n');
    if (lines.isEmpty()) {
        socket->write(response(400, "text/plain", "Bad request.\r\n"));
        return;
    }
    const QList<QByteArray> requestLine = lines[0].trimmed().split(' ');
    if (requestLine.size() < 2) {
        socket->write(response(400, "text/plain", "Bad request.\r\n"));
        return;
    }
    const QByteArray method = requestLine[0];
    const QByteArray pathAndQuery = requestLine[1];

    // Only GET / HEAD are supported.
    if (method != "GET" && method != "HEAD") {
        socket->write(response(405, "text/plain", "Method not allowed.\r\n"));
        return;
    }

    // Extract the Authorization header.
    QString authHeader;
    for (const QByteArray& line : lines.mid(1)) {
        if (line.startsWith("Authorization:")) {
            authHeader = QString::fromLatin1(line.mid(14).trimmed());
            break;
        }
    }

    const QUrl url(QString::fromLatin1(pathAndQuery));
    const QString path = url.path();

    // ── Authentication ───────────────────────────────────────────
    if (!authorized(authHeader, url)) {
        noteFailedAuth(peer);
        emit logMessage(QStringLiteral("Remote dashboard: rejected request from %1 (auth)")
                            .arg(peer));
        socket->write(response(401, "application/json",
                               "{\"error\":\"unauthorized\"}\r\n"));
        return;
    }
    if (m_failCount.contains(peer)) m_failCount.remove(peer); // reset on success

    // ── Route ────────────────────────────────────────────────────
    QByteArray body;
    QByteArray contentType;
    if (path == "/api/stats") {
        body = statsJson() + '\n';
        contentType = "application/json";
    } else if (path == "/api/daily") {
        body = dailyJson() + '\n';
        contentType = "application/json";
    } else if (path == "/health") {
        body = healthJson() + '\n';
        contentType = "application/json";
    } else if (path == "/" || path.isEmpty()) {
        body = dashboardHtml();
        contentType = "text/html; charset=utf-8";
    } else {
        socket->write(response(404, "text/plain", "Not found.\r\n"));
        return;
    }

    if (method == "HEAD") body.clear();
    socket->write(response(200, contentType.constData(), body));
}

void RemoteServer::noteFailedAuth(const QString& peer)
{
    const int fails = m_failCount.value(peer, 0) + 1;
    m_failCount.insert(peer, fails);
    const qint64 now = QDateTime::currentMSecsSinceEpoch();
    if (fails >= 5) m_lockUntil.insert(peer, now + 10 * 60 * 1000);
}

bool RemoteServer::isLockedOut(const QString& peer) const
{
    const qint64 lock = m_lockUntil.value(peer, 0);
    if (lock == 0) return false;
    if (QDateTime::currentMSecsSinceEpoch() < lock) return true;
    return false;
}

bool RemoteServer::authorized(const QString& authHeader, const QUrl& url) const
{
    if (m_token.isEmpty()) return false;

    QByteArray candidate;
    if (authHeader.startsWith("Bearer ", Qt::CaseInsensitive)) {
        candidate = authHeader.mid(7).trimmed().toUtf8();
    }
    if (candidate.isEmpty()) {
        const QString qtok = QUrlQuery(url).queryItemValue("token");
        if (!qtok.isEmpty()) candidate = qtok.toUtf8();
    }
    return !candidate.isEmpty() && constantTimeEquals(candidate, m_token);
}

bool RemoteServer::constantTimeEquals(const QByteArray& a, const QByteArray& b)
{
    if (a.size() != b.size()) return false;
    unsigned char diff = 0;
    for (int i = 0; i < a.size(); ++i) {
        diff |= static_cast<unsigned char>(a.at(i) ^ b.at(i));
    }
    return diff == 0;
}

QByteArray RemoteServer::response(int status, const char* contentType,
                                  const QByteArray& body) const
{
    const char* reason = "OK";
    switch (status) {
    case 404: reason = "Not Found"; break;
    case 401: reason = "Unauthorized"; break;
    case 405: reason = "Method Not Allowed"; break;
    case 400: reason = "Bad Request"; break;
    case 408: reason = "Request Timeout"; break;
    case 429: reason = "Too Many Requests"; break;
    case 431: reason = "Request Header Fields Too Large"; break;
    case 503: reason = "Service Unavailable"; break;
    default: break;
    }

    QByteArray out;
    out += "HTTP/1.1 " + QByteArray::number(status) + " " + reason + "\r\n";
    out += "Content-Type: " + QByteArray(contentType) + "\r\n";
    out += "Content-Length: " + QByteArray::number(body.size()) + "\r\n";
    out += "Cache-Control: no-store\r\n";
    out += "Connection: close\r\n";
    out += "X-Content-Type-Options: nosniff\r\n";
    // Defense in depth: never leak the (possibly query-string) token via a
    // Referer header, and refuse to load any external resource.
    out += "Referrer-Policy: no-referrer\r\n";
    out += "Content-Security-Policy: default-src 'none'; style-src 'unsafe-inline'; "
           "base-uri 'none'; form-action 'none'\r\n";
    out += "\r\n";
    out += body;
    return out;
}

// ── Data builders ──────────────────────────────────────────────────

QByteArray RemoteServer::statsJson() const
{
    const auto allSales = m_db.getAllSales();
    const auto items = m_db.getAllItems();
    const auto users = m_db.getAllUsers();

    std::unordered_map<std::string, std::string> iNames, uNames;
    for (const auto& it : items) iNames[it.id] = it.name;
    for (const auto& u : users) uNames[u.id] = u.username;

    // Today / month filters.
    const QString todayKey = QDate::currentDate().toString("yyyy-MM-dd");
    const QString monthKey = QDate::currentDate().toString("yyyy-MM");
    std::vector<Domain::Sale> todaySales, monthSales;
    for (const auto& s : allSales) {
        const QString d = QString::fromStdString(Stats::localDay(s));
        if (d == todayKey) todaySales.push_back(s);
        if (d.startsWith(monthKey)) monthSales.push_back(s);
    }

    const auto today = Stats::compute(todaySales, iNames, uNames);
    const auto month = Stats::compute(monthSales, iNames, uNames);
    const auto all = Stats::compute(allSales, iNames, uNames);

    QJsonObject root;
    root.insert("shop", QStringLiteral("QMark"));
    root.insert("version", QString::fromLatin1(QMARK_VERSION_STRING));
    root.insert("generatedAt", QDateTime::currentDateTime().toString(Qt::ISODate));

    auto snapObj = [](const Stats::Snapshot& s) {
        QJsonObject o;
        o.insert("revenue", s.totalRevenue);
        o.insert("transactions", s.totalTx);
        o.insert("itemsSold", s.totalItems);
        return o;
    };
    root.insert("totals", snapObj(all));

    QJsonObject todayObj = snapObj(today);
    if (!today.todayTopSeller.id.isEmpty()) {
        QJsonObject ts;
        ts.insert("id", today.todayTopSeller.id);
        ts.insert("username", today.todayTopSeller.username);
        ts.insert("transactions", today.todayTopSeller.tx);
        ts.insert("revenue", today.todayTopSeller.revenue);
        todayObj.insert("topSeller", ts);
    }
    if (!today.todayTopItem.id.isEmpty()) {
        QJsonObject tii;
        tii.insert("id", today.todayTopItem.id);
        tii.insert("name", today.todayTopItem.name);
        tii.insert("qty", today.todayTopItem.qty);
        tii.insert("revenue", today.todayTopItem.revenue);
        todayObj.insert("topItem", tii);
    }
    root.insert("today", todayObj);

    QJsonObject monthObj = snapObj(month);
    if (!month.sellers.empty() && month.sellers[0].tx > 0) {
        QJsonObject ts;
        ts.insert("id", month.sellers[0].id);
        ts.insert("username", month.sellers[0].username);
        ts.insert("transactions", month.sellers[0].tx);
        ts.insert("revenue", month.sellers[0].revenue);
        monthObj.insert("topSeller", ts);
    }
    if (!month.items.empty() && month.items[0].qty > 0) {
        QJsonObject tii;
        tii.insert("id", month.items[0].id);
        tii.insert("name", month.items[0].name);
        tii.insert("qty", month.items[0].qty);
        tii.insert("revenue", month.items[0].revenue);
        monthObj.insert("topItem", tii);
    }
    root.insert("month", monthObj);

    // Stock overview.
    int lowStock = 0, outOfStock = 0;
    QJsonArray lowStockList;
    for (const auto& it : items) {
        if (Domain::statusIsLowStock(it.status)) {
            ++lowStock;
            lowStockList.append(QString::fromStdString(it.name));
        } else if (Domain::statusIsOutOfStock(it.status)) {
            ++outOfStock;
        }
    }
    root.insert("itemsInStock", int(items.size()));
    root.insert("lowStockCount", lowStock);
    root.insert("outOfStockCount", outOfStock);
    root.insert("lowStockItems", lowStockList);

    // Recent sales (newest 20).
    QJsonArray recent;
    for (std::size_t i = 0; i < allSales.size() && i < 20; ++i) {
        const auto& s = allSales[i];
        QJsonObject so;
        so.insert("id", QString::fromStdString(s.id));
        so.insert("itemId", QString::fromStdString(s.itemId));
        so.insert("item", QString::fromStdString(
            iNames.count(s.itemId) ? iNames.at(s.itemId) : s.itemId));
        so.insert("qty", s.quantitySold);
        so.insert("total", s.totalAmount);
        so.insert("soldBy", QString::fromStdString(
            uNames.count(s.soldBy) ? uNames.at(s.soldBy) : s.soldBy));
        so.insert("when", QString::fromStdString(Domain::toISOString(s.saleDate)));
        recent.append(so);
    }
    root.insert("recentSales", recent);

    return QJsonDocument(root).toJson(QJsonDocument::Compact);
}

QByteArray RemoteServer::dailyJson() const
{
    const auto days = m_db.getDailyStats();
    QJsonArray arr;
    for (const auto& d : days) {
        QJsonObject o;
        o.insert("day", QString::fromStdString(d.day));
        o.insert("transactions", d.tx);
        o.insert("revenue", d.revenue);
        o.insert("itemsSold", d.itemsSold);
        o.insert("bestSellerId", QString::fromStdString(d.bestSellerId));
        o.insert("bestItemId", QString::fromStdString(d.bestItemId));
        arr.append(o);
    }
    QJsonObject root;
    root.insert("days", arr);
    return QJsonDocument(root).toJson(QJsonDocument::Compact);
}

QByteArray RemoteServer::healthJson() const
{
    return "{\"status\":\"ok\",\"version\":\"" + QByteArray(QMARK_VERSION_STRING) + "\"}";
}

QByteArray RemoteServer::dashboardHtml() const
{
    const QByteArray stats = statsJson();
    const QJsonObject root = QJsonDocument::fromJson(stats).object();

    QString h;
    h += "<!DOCTYPE html><html><head><meta charset=\"utf-8\">";
    h += "<meta name=\"viewport\" content=\"width=device-width, initial-scale=1\">";
    h += "<title>QMark — Shop Dashboard</title><style>";
    h += "body{font-family:Segoe UI,Roboto,sans-serif;background:#f0f2f5;margin:0;padding:24px;color:#222;}";
    h += ".wrap{max-width:880px;margin:0 auto;}";
    h += ".card{background:#fff;border-radius:10px;padding:18px 22px;margin-bottom:16px;box-shadow:0 1px 3px rgba(0,0,0,.12);}";
    h += "h1{font-size:22px;margin:0 0 4px;} h2{font-size:16px;margin:0 0 10px;color:#555;}";
    h += "table{width:100%;border-collapse:collapse;font-size:14px;} td,th{padding:6px 8px;text-align:left;border-bottom:1px solid #eee;}";
    h += ".num{font-weight:700;color:#0078d4;} .bad{color:#c62828;} .ok{color:#2e7d32;}";
    h += "</style></head><body><div class=\"wrap\">";
    h += "<h1>QMark — Shop Dashboard</h1><h2>Version "
      + htmlEscape(QString::fromLatin1(QMARK_VERSION_STRING))
      + " · generated " + htmlEscape(QDateTime::currentDateTime().toString("yyyy-MM-dd hh:mm:ss"))
      + "</h2>";

    // Totals
    h += "<div class=\"card\"><h2>Totals</h2><table>";
    h += "<tr><td>Total revenue</td><td class=\"num\">"
       + Stats::formatMoney(root.value("totals").toObject().value("revenue").toDouble())
       + "</td></tr>";
    h += "<tr><td>Transactions</td><td>"
       + QString::number(root.value("totals").toObject().value("transactions").toInt())
       + "</td></tr>";
    h += "<tr><td>Items sold</td><td>"
       + QString::number(root.value("totals").toObject().value("itemsSold").toInt())
       + "</td></tr>";
    h += "</table></div>";

    // Today
    const QJsonObject today = root.value("today").toObject();
    h += "<div class=\"card\"><h2>Today</h2><table>";
    h += "<tr><td>Revenue</td><td class=\"num\">"
       + Stats::formatMoney(today.value("revenue").toDouble()) + "</td></tr>";
    h += "<tr><td>Transactions</td><td>"
       + QString::number(today.value("transactions").toInt()) + "</td></tr>";
    h += "<tr><td>Items sold</td><td>"
       + QString::number(today.value("itemsSold").toInt()) + "</td></tr>";
    const QJsonObject ts = today.value("topSeller").toObject();
    if (!ts.isEmpty()) {
        h += "<tr><td>Top seller</td><td>" + htmlEscape(ts.value("username").toString())
           + " (" + QString::number(ts.value("transactions").toInt()) + " tx)</td></tr>";
    }
    const QJsonObject tii = today.value("topItem").toObject();
    if (!tii.isEmpty()) {
        h += "<tr><td>Top item</td><td>" + htmlEscape(tii.value("name").toString())
           + " ×" + QString::number(tii.value("qty").toInt()) + "</td></tr>";
    }
    h += "</table></div>";

    // Month
    const QJsonObject mon = root.value("month").toObject();
    h += "<div class=\"card\"><h2>This month</h2><table>";
    h += "<tr><td>Revenue</td><td class=\"num\">"
       + Stats::formatMoney(mon.value("revenue").toDouble()) + "</td></tr>";
    h += "<tr><td>Transactions</td><td>"
       + QString::number(mon.value("transactions").toInt()) + "</td></tr>";
    h += "<tr><td>Items sold</td><td>"
       + QString::number(mon.value("itemsSold").toInt()) + "</td></tr>";
    h += "</table></div>";

    // Stock
    h += "<div class=\"card\"><h2>Stock</h2><table>";
    h += QString("<tr><td>Items in stock</td><td>%1</td></tr>").arg(root.value("itemsInStock").toInt());
    h += QString("<tr><td>Low stock</td><td class=\"%1\">%2</td></tr>")
             .arg(root.value("lowStockCount").toInt() > 0 ? "bad" : "ok")
             .arg(root.value("lowStockCount").toInt());
    h += QString("<tr><td>Out of stock</td><td class=\"%1\">%2</td></tr>")
             .arg(root.value("outOfStockCount").toInt() > 0 ? "bad" : "ok")
             .arg(root.value("outOfStockCount").toInt());
    h += "</table></div>";

    // Recent sales
    const QJsonArray recent = root.value("recentSales").toArray();
    h += "<div class=\"card\"><h2>Recent sales</h2><table>"
         "<tr><th>When</th><th>Item</th><th>Qty</th><th>Total</th><th>By</th></tr>";
    for (const auto& v : recent) {
        const QJsonObject so = v.toObject();
        h += "<tr><td>" + htmlEscape(so.value("when").toString()) + "</td>"
           + "<td>" + htmlEscape(so.value("item").toString()) + "</td>"
           + "<td>" + QString::number(so.value("qty").toInt()) + "</td>"
           + "<td>" + Stats::formatMoney(so.value("total").toDouble()) + "</td>"
           + "<td>" + htmlEscape(so.value("soldBy").toString()) + "</td></tr>";
    }
    h += "</table></div>";

    h += "<p style=\"color:#888;font-size:12px;\">Read-only dashboard · JSON API: /api/stats, /api/daily</p>";
    h += "</div></body></html>";
    return h.toUtf8();
}
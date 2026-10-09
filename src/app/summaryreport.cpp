#include "summaryreport.h"

#include <QDate>
#include <QDateTime>
#include <algorithm>
#include <unordered_map>

#include "statistics.h"
#include "translations.h"

namespace Summary {

namespace {

// Map item/user IDs to display names for Stats::compute.
std::unordered_map<std::string, std::string> itemNames(const std::vector<Domain::Item>& items)
{
    std::unordered_map<std::string, std::string> m;
    for (const auto& it : items) m[it.id] = it.name;
    return m;
}

std::unordered_map<std::string, std::string> userNames(const std::vector<Domain::User>& users)
{
    std::unordered_map<std::string, std::string> m;
    for (const auto& u : users) m[u.id] = u.username;
    return m;
}

// Filter sales to the requested period.
std::vector<Domain::Sale> filteredSales(const std::vector<Domain::Sale>& all, bool monthly)
{
    std::vector<Domain::Sale> out;
    const QString todayKey = QDate::currentDate().toString("yyyy-MM-dd");
    const QString monthKey = QDate::currentDate().toString("yyyy-MM");
    for (const auto& s : all) {
        const QString day = QString::fromStdString(Stats::localDay(s));
        if (!monthly) {
            if (day == todayKey) out.push_back(s);
        } else {
            if (day.startsWith(monthKey)) out.push_back(s);
        }
    }
    return out;
}

QString htmlEscape(const QString& s)
{
    QString out = s;
    out.replace('&', "&amp;").replace('<', "&lt;").replace('>', "&gt;");
    return out;
}

} // namespace

QString buildText(bool monthly, DataAccess::IDataAccess& db)
{
    const auto allSales = db.getAllSales();
    const auto sales = filteredSales(allSales, monthly);
    const auto items = db.getAllItems();
    const auto users = db.getAllUsers();
    const auto snap = Stats::compute(sales, itemNames(items), userNames(users));

    QString t;
    t += "═══════════════════════════════════════\n";
    t += "   QMARK — " + Tr::trS(monthly ? "MONTHLY SHOP SUMMARY" : "DAILY SHOP SUMMARY") + "\n";
    t += "═══════════════════════════════════════\n";
    t += Tr::trS("Generated: ") + QDateTime::currentDateTime().toString("yyyy-MM-dd hh:mm:ss") + "\n";
    t += Tr::trS("Period: ") + (monthly
        ? QDate::currentDate().toString("MMMM yyyy")
        : QDate::currentDate().toString("yyyy-MM-dd")) + "\n\n";

    t += Tr::trS("Total Revenue: ") + Stats::formatMoney(snap.totalRevenue) + "\n";
    t += Tr::trS("Total Transactions: ") + QString::number(snap.totalTx) + "\n";
    t += Tr::trS("Total Items Sold: ") + QString::number(snap.totalItems) + "\n\n";

    if (!monthly) {
        QString topSeller = "-";
        if (!snap.todayTopSeller.id.isEmpty())
            topSeller = snap.todayTopSeller.username + " — "
                      + QString::number(snap.todayTopSeller.tx) + " " + Tr::trS("tx, ")
                      + Stats::formatMoney(snap.todayTopSeller.revenue);
        QString topItem = "-";
        if (!snap.todayTopItem.id.isEmpty())
            topItem = snap.todayTopItem.name + " — "
                    + QString::number(snap.todayTopItem.qty) + " " + Tr::trS("pcs, ")
                    + Stats::formatMoney(snap.todayTopItem.revenue);
        t += Tr::trS("Top Seller (today): ") + topSeller + "\n";
        t += Tr::trS("Top Item (today): ") + topItem + "\n\n";
    } else {
        QString topSeller = "-", topItem = "-";
        if (!snap.sellers.empty() && snap.sellers[0].tx > 0)
            topSeller = snap.sellers[0].username + " — "
                      + QString::number(snap.sellers[0].tx) + " " + Tr::trS("tx, ")
                      + Stats::formatMoney(snap.sellers[0].revenue);
        if (!snap.items.empty() && snap.items[0].qty > 0)
            topItem = snap.items[0].name + " — "
                    + QString::number(snap.items[0].qty) + " " + Tr::trS("pcs, ")
                    + Stats::formatMoney(snap.items[0].revenue);
        t += Tr::trS("Top Seller (month): ") + topSeller + "\n";
        t += Tr::trS("Top Item (month): ") + topItem + "\n\n";
    }

    // Low stock alerts.
    QStringList lowStockList;
    int outOfStock = 0;
    for (const auto& it : items) {
        if (Domain::statusIsLowStock(it.status)) lowStockList << QString::fromStdString(it.name);
        if (Domain::statusIsOutOfStock(it.status)) outOfStock++;
    }
    t += Tr::trS("Items in stock: ") + QString::number(items.size()) + "\n";
    t += Tr::trS("Low Stock: ") + QString::number(lowStockList.size()) + "\n";
    t += Tr::trS("Out of Stock: ") + QString::number(outOfStock) + "\n";
    if (!lowStockList.isEmpty()) {
        t += "\n" + Tr::trS("Low stock items needing replenishment:") + "\n";
        for (const QString& n : lowStockList) t += "  • " + n + "\n";
    }
    t += "\n";
    t += "───────────────────────────────────────\n";
    t += Tr::trS("End of summary") + "\n";
    return t;
}

QString buildHtml(bool monthly, DataAccess::IDataAccess& db)
{
    const QString text = buildText(monthly, db);
    QString h;
    h += "<html><head><meta charset=\"utf-8\"><style>"
         "body{font-family:sans-serif;color:#222;background:#f7f7f7;padding:20px;}"
         "pre{background:#fff;border:1px solid #ddd;border-radius:8px;padding:16px;"
         "white-space:pre-wrap;font-family:monospace;}"
         "</style></head><body><h3>QMark — "
      + htmlEscape(Tr::trS(monthly ? "MONTHLY SHOP SUMMARY" : "DAILY SHOP SUMMARY"))
      + "</h3><pre>" + htmlEscape(text) + "</pre></body></html>";
    return h;
}

} // namespace Summary
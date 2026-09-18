#pragma once

// ─────────────────────────────────────────────────────────────────────
// statistics.h — GUI-independent shop statistics engine.
//
// All statistics aggregation, local-date/time handling, money
// formatting and report-text generation live here, with no QtWidgets
// dependency (QtCore only). The POS software is switched off during
// breaks, so statistics must NOT depend on the running GUI:
//   * the data layer persists one daily snapshot per LOCAL day into the
//     items database automatically on every sale (see upsertDailyStat),
//   * this module recomputes everything from the database on demand,
//   * the GUI only binds the results to labels/charts.
// ─────────────────────────────────────────────────────────────────────

#include <QString>
#include <QStringList>
#include <QDateTime>
#include <QDate>
#include <QLocale>
#include <string>
#include <vector>
#include <map>
#include <unordered_map>
#include <algorithm>

#include "domain.h"
#include "translations.h"

namespace Stats {

// ── Local time conversions ─────────────────────────────────────────
// Timestamps are stored as UTC ISO strings; statistics aggregate by the
// shop's LOCAL day/hour so "today"/"yesterday" and the activity chart
// match what the shop actually experiences.

inline QDateTime toLocalDateTime(const Domain::DateTime& dt)
{
    return QDateTime::fromString(
        QString::fromStdString(Domain::toISOString(dt)), Qt::ISODate)
        .toLocalTime();
}

inline std::string localDay(const Domain::DateTime& dt)
{
    return toLocalDateTime(dt).date().toString("yyyy-MM-dd").toStdString();
}

inline std::string localDay(const Domain::Sale& s) { return localDay(s.saleDate); }

inline int localHour(const Domain::Sale& s)
{
    return toLocalDateTime(s.saleDate).time().hour();
}

// ── Money formatting (UI + report) ─────────────────────────────────
inline QString formatMoney(double value)
{
    if (Domain::currencySymbol() == "zł") {
        return QLocale(QLocale::Polish).toCurrencyString(value, "zł");
    }
    return QLocale(QLocale::English).toCurrencyString(value, "$");
}

// ── Aggregated snapshot ────────────────────────────────────────────
struct SellerStat { QString id, username; int tx = 0; double revenue = 0.0; };
struct ItemStat   { QString id, name; int qty = 0; double revenue = 0.0; };

struct Snapshot {
    double totalRevenue = 0.0;
    int totalTx = 0;
    int totalItems = 0;

    std::map<QString, double> revByDay;   // local "yyyy-MM-dd" -> revenue
    std::map<QString, int> qtyByItem;     // itemId -> qty bought (all time)
    std::map<QString, double> revByItem;  // itemId -> revenue (all time)

    std::vector<SellerStat> sellers;      // ranked by revenue, descending
    std::vector<ItemStat> items;          // ranked by qty, descending
    std::vector<int> hourlyActivity;      // 24 entries, all-time sales per hour

    // Today (local calendar day)
    int todayTx = 0;
    double todayRevenue = 0.0;
    int todayItems = 0;
    SellerStat todayTopSeller;
    ItemStat todayTopItem;
    int todayBusiestHour = -1;            // local hour, -1 when no sales today
    int todayBusiestHourCount = 0;
};

// One aggregation pass over all sales → everything the GUI and reports
// need. Pure data; fully usable without a running GUI.
inline Snapshot compute(
    const std::vector<Domain::Sale>& sales,
    const std::unordered_map<std::string, std::string>& itemNames,
    const std::unordered_map<std::string, std::string>& userNames)
{
    Snapshot snap;
    snap.hourlyActivity.assign(24, 0);

    const QString todayKey = QDate::currentDate().toString("yyyy-MM-dd");

    std::unordered_map<std::string, std::pair<int, double>> sellerAgg;      // id -> (tx, revenue)
    std::unordered_map<std::string, std::pair<int, double>> sellerAggToday;
    std::unordered_map<std::string, int> itemQtyToday;
    int hourCountToday[24] = { 0 };

    for (const auto& s : sales) {
        snap.totalRevenue += s.totalAmount;
        snap.totalTx++;
        snap.totalItems += s.quantitySold;

        QString iid = QString::fromStdString(s.itemId);
        snap.qtyByItem[iid] += s.quantitySold;
        snap.revByItem[iid] += s.totalAmount;
        snap.revByDay[QString::fromStdString(localDay(s))] += s.totalAmount;

        auto& agg = sellerAgg[s.soldBy];
        agg.first++;
        agg.second += s.totalAmount;

        int h = localHour(s);
        if (h >= 0 && h < 24) snap.hourlyActivity[h]++;

        if (QString::fromStdString(localDay(s)) == todayKey) {
            snap.todayTx++;
            snap.todayRevenue += s.totalAmount;
            snap.todayItems += s.quantitySold;
            auto& tAgg = sellerAggToday[s.soldBy];
            tAgg.first++;
            tAgg.second += s.totalAmount;
            itemQtyToday[s.itemId] += s.quantitySold;
            if (h >= 0 && h < 24) hourCountToday[h]++;
        }
    }

    // Rank sellers by revenue, items by quantity (all time).
    for (const auto& kv : sellerAgg) {
        SellerStat st;
        st.id = QString::fromStdString(kv.first);
        st.username = userNames.count(kv.first)
            ? QString::fromStdString(userNames.at(kv.first)) : st.id;
        st.tx = kv.second.first;
        st.revenue = kv.second.second;
        snap.sellers.push_back(std::move(st));
    }
    std::sort(snap.sellers.begin(), snap.sellers.end(),
        [](const SellerStat& a, const SellerStat& b) { return a.revenue > b.revenue; });

    for (const auto& kv : snap.qtyByItem) {
        ItemStat st;
        st.id = kv.first;
        const std::string iid = st.id.toStdString();
        st.name = itemNames.count(iid) ? QString::fromStdString(itemNames.at(iid)) : st.id;
        st.qty = kv.second;
        auto revIt = snap.revByItem.find(kv.first);
        st.revenue = revIt != snap.revByItem.end() ? revIt->second : 0.0;
        snap.items.push_back(std::move(st));
    }
    std::sort(snap.items.begin(), snap.items.end(),
        [](const ItemStat& a, const ItemStat& b) { return a.qty > b.qty; });

    // Today's top seller (highest revenue today).
    for (const auto& kv : sellerAggToday) {
        if (kv.second.second > snap.todayTopSeller.revenue) {
            snap.todayTopSeller.id = QString::fromStdString(kv.first);
            snap.todayTopSeller.username = userNames.count(kv.first)
                ? QString::fromStdString(userNames.at(kv.first)) : snap.todayTopSeller.id;
            snap.todayTopSeller.tx = kv.second.first;
            snap.todayTopSeller.revenue = kv.second.second;
        }
    }

    // Today's top item (most units sold today).
    for (const auto& kv : itemQtyToday) {
        if (kv.second > snap.todayTopItem.qty) {
            const QString kq = QString::fromStdString(kv.first);
            snap.todayTopItem.id = kq;
            snap.todayTopItem.name = itemNames.count(kv.first)
                ? QString::fromStdString(itemNames.at(kv.first)) : kq;
            snap.todayTopItem.qty = kv.second;
            auto revIt = snap.revByItem.find(kq);
            snap.todayTopItem.revenue = revIt != snap.revByItem.end() ? revIt->second : 0.0;
        }
    }

    // Today's busiest local hour.
    for (int h = 0; h < 24; ++h) {
        if (hourCountToday[h] > snap.todayBusiestHourCount) {
            snap.todayBusiestHourCount = hourCountToday[h];
            snap.todayBusiestHour = h;
        }
    }

    return snap;
}

// ── Localised report text (EN/PL) ──────────────────────────────────
// Reproduces the full sales report from a Snapshot + the item list,
// without any GUI dependency.
inline QString buildReportText(const Snapshot& snap, const std::vector<Domain::Item>& items)
{
    QString report;
    report += "═══════════════════════════════════════\n";
    report += "         QMARK — " + Tr::trS("SALES REPORT") + "\n";
    report += "═══════════════════════════════════════\n";
    report += Tr::trS("Generated: ") + QDateTime::currentDateTime().toString("yyyy-MM-dd hh:mm:ss") + "\n\n";
    report += Tr::trS("Total Revenue: ") + formatMoney(snap.totalRevenue) + "\n";
    report += Tr::trS("Total Transactions: ") + QString::number(snap.totalTx) + "\n";
    report += Tr::trS("Total Items Sold: ") + QString::number(snap.totalItems) + "\n\n";

    report += Tr::trS("Items in stock: ") + QString::number(items.size()) + "\n";
    int lowStock = 0, outOfStock = 0;
    QStringList lowStockList;
    for (const auto& item : items) {
        if (item.status == "Low Stock") { lowStock++; lowStockList << QString::fromStdString(item.name); }
        if (item.status == "Out of Stock" || item.status == "Sold Out") outOfStock++;
    }
    report += Tr::trS("Low Stock: ") + QString::number(lowStock) + "\n";
    report += Tr::trS("Out of Stock: ") + QString::number(outOfStock) + "\n\n";

    // ── Today's statistics ──────────────────────────────────────
    report += "───────────────────────────────────────\n";
    report += Tr::trS("TODAY'S STATISTICS") + "\n";
    report += "───────────────────────────────────────\n";
    report += Tr::trS("Today's Sales: ") + QString::number(snap.todayTx) + "\n";
    report += Tr::trS("Today's Revenue: ") + formatMoney(snap.todayRevenue) + "\n";
    report += Tr::trS("Today's Items Sold: ") + QString::number(snap.todayItems) + "\n";

    QString topSellerToday = "-";
    if (!snap.todayTopSeller.id.isEmpty()) {
        topSellerToday = snap.todayTopSeller.username + " — "
                       + QString::number(snap.todayTopSeller.tx) + " " + Tr::trS("tx, ")
                       + formatMoney(snap.todayTopSeller.revenue);
    }
    report += Tr::trS("Today's Top Seller: ") + topSellerToday + "\n";

    QString topItemToday = "-";
    if (!snap.todayTopItem.id.isEmpty()) {
        topItemToday = snap.todayTopItem.name + " — "
                     + QString::number(snap.todayTopItem.qty) + " " + Tr::trS("pcs");
    }
    report += Tr::trS("Today's Top Item: ") + topItemToday + "\n";

    QString busiestHour = "-";
    if (snap.todayBusiestHourCount > 0) {
        busiestHour = QString("%1:00").arg(snap.todayBusiestHour) + " ("
                    + QString::number(snap.todayBusiestHourCount) + " " + Tr::trS("sales") + ")";
    }
    report += Tr::trS("Today's Busiest Hour: ") + busiestHour + "\n";
    report += "\n";

    // ── Trends ──────────────────────────────────────────────────
    report += "───────────────────────────────────────\n";
    report += Tr::trS("TRENDS") + "\n";
    report += "───────────────────────────────────────\n";

    report += Tr::trS("Revenue by day (last 7 days):") + "\n";
    QDate todayLocal = QDate::currentDate();
    auto dayRev = [&](const QString& day) -> double {
        auto it = snap.revByDay.find(day);
        return it != snap.revByDay.end() ? it->second : 0.0;
    };
    bool anyDay = false;
    for (int d = 6; d >= 0; --d) {
        QString day = todayLocal.addDays(-d).toString("yyyy-MM-dd");
        double rev = dayRev(day);
        if (rev <= 0.0 && d != 0) continue;
        anyDay = true;
        report += QString("  %1  %2\n").arg(day).arg(rev > 0.0 ? formatMoney(rev) : formatMoney(0.0));
    }
    if (!anyDay && snap.revByDay.empty()) report += Tr::trS("  (no sales recorded yet)") + "\n";
    double yRev = dayRev(todayLocal.addDays(-1).toString("yyyy-MM-dd"));
    double todayRev = dayRev(todayLocal.toString("yyyy-MM-dd"));
    if (yRev > 0.0 && todayRev > 0.0) {
        double pct = ((todayRev - yRev) / yRev) * 100.0;
        report += Tr::trS("Trend vs yesterday: ") + QString("%1%").arg(pct, 0, 'f', 1) +
                  (pct >= 0.0 ? " " + Tr::trS("(up)") : " " + Tr::trS("(down)")) + "\n";
    } else if (todayRev > 0.0) {
        report += Tr::trS("Trend: no sales yesterday (new activity today)") + "\n";
    }
    report += "\n";

    report += Tr::trS("Top selling items:") + "\n";
    if (snap.items.empty()) {
        report += Tr::trS("  (no sales yet)") + "\n";
    } else {
        int shown = 0;
        for (const auto& p : snap.items) {
            if (++shown > 5) break;
            report += QString("  %1. %2 — %3 ").arg(shown).arg(p.name).arg(p.qty) +
                      Tr::trS("pcs, ") + formatMoney(p.revenue) + "\n";
        }
    }
    report += "\n";

    report += Tr::trS("Top sellers (staff):") + "\n";
    if (snap.sellers.empty()) {
        report += Tr::trS("  (no sales yet)") + "\n";
    } else {
        int shown = 0;
        for (const auto& s : snap.sellers) {
            if (++shown > 3) break;
            report += QString("  %1. %2 — %3 ").arg(shown).arg(s.username).arg(s.tx) +
                      Tr::trS("tx, ") + formatMoney(s.revenue) + "\n";
        }
    }
    report += "\n";

    if (!lowStockList.isEmpty()) {
        report += Tr::trS("Low stock items needing replenishment:") + "\n";
        for (const QString& n : lowStockList) report += "  • " + n + "\n";
        report += "\n";
    }

    return report;
}

} // namespace Stats
#pragma once

// ─────────────────────────────────────────────────────────────────────
// pdf_export.h — dependency-free report → PDF exporter.
//
// Writes the generated report as A4 PDF: the report text first (monospace,
// pre-wrapped lines kept intact), then a snapshot of every chart widget in
// `chartsContainer`, starting a new page whenever content no longer fits.
// Uses only QtGui/QtWidgets (QPdfWriter), so no extra Qt module is needed
// in the static Windows cross-build.
// ─────────────────────────────────────────────────────────────────────

#include <QString>
#include <QWidget>
#include <QLayout>
#include <QImage>
#include <QPainter>
#include <QFontMetrics>
#include <QPdfWriter>
#include <QPageSize>
#include <QPageLayout>
#include <QMarginsF>
#include <QtMath>

inline bool exportReportToPdf(const QString& fileName,
                              const QString& reportText,
                              QWidget* chartsContainer,
                              QString* errorMessage = nullptr)
{
    if (reportText.isEmpty()) {
        if (errorMessage) *errorMessage = QStringLiteral("empty report");
        return false;
    }

    const int res = 96;  // device pixels ≈ screen px; A4 = 794×1123 px
    QPdfWriter writer(fileName);
    writer.setPageSize(QPageSize(QPageSize::A4));
    writer.setPageMargins(QMarginsF(15, 15, 15, 15), QPageLayout::Millimeter);
    writer.setResolution(res);

    QPainter painter(&writer);
    if (!painter.isActive()) {
        if (errorMessage) *errorMessage = QStringLiteral("failed to open PDF");
        return false;
    }
    painter.setRenderHint(QPainter::Antialiasing);

    const int pageW = writer.width();
    const int pageH = writer.height();
    const int margin = qRound(15.0 / 25.4 * res);   // 15 mm → pixels
    const int left   = margin;
    const int top    = margin;
    const int maxX   = pageW - margin;
    const int maxY   = pageH - margin;

    // ── Report text (monospace, lines kept intact) ────────────────
    QFont mono(QStringLiteral("Courier New"), 10);
    const QFontMetrics fm(mono);
    const int lineH = fm.height() + 2;
    painter.setFont(mono);

    int y = top;
    const auto newPageIfNeeded = [&](int needed) {
        if (y + needed > maxY) {
            writer.newPage();
            y = top;
        }
    };

    const QStringList lines = reportText.split('\n');
    for (const QString& raw : lines) {
        QString remain = raw;
        while (!remain.isEmpty()) {
            newPageIfNeeded(lineH);
            int x = left;
            // Wrap any over-long line to the printable width.
            QString fit = remain;
            while (fm.horizontalAdvance(fit) > (maxX - x) && fit.size() > 1) fit.chop(1);
            painter.drawText(x, y, fit);
            remain = remain.mid(fit.size());
            y += lineH;
        }
    }

    // ── Charts (snapshots of the report chart widgets) ────────────
    if (chartsContainer) {
        QLayout* lay = chartsContainer->layout();
        const int availW = maxX - left;
        for (int i = 0; lay && i < lay->count(); ++i) {
            QLayoutItem* it = lay->itemAt(i);
            QWidget* w = it ? it->widget() : nullptr;
            if (!w) continue;
            QImage img = w->grab().toImage();
            if (img.isNull()) continue;
            QImage scaled = (availW < img.width())
                ? img.scaledToWidth(availW, Qt::SmoothTransformation)
                : img;
            newPageIfNeeded(scaled.height() + 8);
            painter.drawImage(left, y, scaled);
            y += scaled.height() + 8;
        }
    }

    painter.end();
    return true;
}
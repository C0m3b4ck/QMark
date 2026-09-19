#pragma once

// ─────────────────────────────────────────────────────────────────────
// pdf_export.h — dependency-free report → PDF exporter.
//
// Writes the generated report as A4 PDF: the report text first (clean
// sans-serif body, pre-wrapped lines kept intact), then a snapshot of
// every chart widget marked with the "reportChart" property inside
// `chartsContainer` (which may be a plain container or a QSplitter),
// starting a new page whenever content no longer fits.
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

    // ── Report text (sans-serif body, lines kept intact) ─────────
    // A clean humanist sans instead of the old terminal-looking mono:
    // QPdfWriter embeds TrueType glyphs (verified: /FontFile2), so any
    // installed font family renders correctly in the static build too.
    QFont font(QStringLiteral("Segoe UI"), 11);
    const QFontMetrics fm(font);
    const int lineH = fm.height() + 3;
    painter.setFont(font);

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
        // The chart widgets are direct children of a vertical QSplitter;
        // they are marked with the "reportChart" property so the splitter
        // handles are skipped without depending on widget class names.
        const int availW = maxX - left;
        const QList<QWidget*> kids =
            chartsContainer->findChildren<QWidget*>(QString(), Qt::FindDirectChildrenOnly);
        for (QWidget* w : kids) {
            if (!w || !w->property("reportChart").toBool()) continue;
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
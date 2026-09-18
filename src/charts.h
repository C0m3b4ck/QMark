#pragma once

// ─────────────────────────────────────────────────────────────────────
// charts.h — lightweight statistics charts for the QMark report page.
//
// Header-only painted widgets (no Q_OBJECT, so no moc is needed):
//   * PieChartWidget  — "which items were sold the most" (composition)
//   * BarChartWidget  — "when did the shop receive the most activity"
// Both are plain QWidget subclasses with a styleable, dependency-free
// drawing routine so they work in the static Windows cross-build too.
// ─────────────────────────────────────────────────────────────────────

#include <QWidget>
#include <QVector>
#include <QPair>
#include <QString>
#include <QPainter>
#include <QPaintEvent>
#include <QFont>
#include <QtMath>

// ── Shared palette ──────────────────────────────────────────────────
namespace ChartPalette {
inline const char* const colors[10] = {
    "#e53935", "#1e88e5", "#43a047", "#fdd835", "#8e24aa",
    "#fb8c00", "#00acc1", "#5e35b1", "#795548", "#26a69a"
};
}

// ── Pie chart: share of each slice ──────────────────────────────────
class PieChartWidget : public QWidget
{
public:
    explicit PieChartWidget(QWidget* parent = nullptr)
        : QWidget(parent)
    {
        setMinimumSize(280, 240);
    }

    void setTitle(const QString& t) { m_title = t; update(); }
    void setData(const QVector<QPair<QString, double>>& slices)
    {
        m_slices = slices;
        double total = 0.0;
        for (const auto& s : slices) total += s.second;
        m_total = total;
        update();
    }

protected:
    void paintEvent(QPaintEvent*) override
    {
        QPainter p(this);
        p.setRenderHint(QPainter::Antialiasing);
        p.fillRect(rect(), Qt::white);

        QFont titleFont = font();
        titleFont.setPointSize(11);
        titleFont.setBold(true);
        p.setFont(titleFont);
        p.setPen(QColor("#333333"));
        p.drawText(QRect(0, 8, width(), 24), Qt::AlignHCenter | Qt::AlignVCenter, m_title);

        if (m_slices.isEmpty() || m_total <= 0.0) {
            QFont f = font();
            f.setPointSize(10);
            p.setFont(f);
            p.setPen(QColor("#888888"));
            p.drawText(QRect(0, height() / 2 - 12, width(), 24),
                       Qt::AlignCenter, "—");
            return;
        }

        // Pie occupies the left ~52%, legend the right side.
        int legendW = width() * 2 / 5;
        int pieSide = qMin(width() - legendW - 20, height() - 70);
        QRectF pieRect(width() - legendW - 10 - pieSide,
                       (height() - pieSide) / 2 + 6,
                       pieSide, pieSide);

        double start = 90.0 * 16;                    // start at 12 o'clock
        for (int i = 0; i < m_slices.size(); ++i) {
            double frac = m_slices[i].second / m_total;
            int span = int(frac * 360.0 * 16.0);
            if (span <= 1) span = 1;
            p.setBrush(QColor(QString::fromLatin1(ChartPalette::colors[i % 10])));
            p.setPen(QPen(Qt::white, 2));
            p.drawPie(pieRect, int(start), span);
            start += span;
        }

        // Legend
        QFont f = font();
        f.setPointSize(9);
        p.setFont(f);
        int rowH = 20;
        int y = (height() - int(m_slices.size()) * rowH) / 2 + 12;
        int lx = width() - legendW + 8;
        for (int i = 0; i < m_slices.size(); ++i) {
            p.setBrush(QColor(QString::fromLatin1(ChartPalette::colors[i % 10])));
            p.setPen(Qt::NoPen);
            p.drawRect(lx, y + 2, 12, 12);
            double pct = m_total > 0.0 ? (m_slices[i].second / m_total * 100.0) : 0.0;
            p.setPen(QColor("#333333"));
            p.drawText(QRect(lx + 18, y - 2, legendW - 26, 18),
                       Qt::AlignLeft | Qt::AlignVCenter,
                       QString("%1  (%2%)").arg(m_slices[i].first).arg(pct, 0, 'f', 1));
            y += rowH;
        }
    }

private:
    QString m_title;
    QVector<QPair<QString, double>> m_slices;
    double m_total = 0.0;
};

// ── Bar chart: activity per slot (e.g. sales per hour) ──────────────
class BarChartWidget : public QWidget
{
public:
    explicit BarChartWidget(QWidget* parent = nullptr)
        : QWidget(parent)
    {
        setMinimumSize(280, 240);
    }

    void setTitle(const QString& t) { m_title = t; update(); }
    // values: one entry per bar; label drawn under every `labelEvery`th.
    void setData(const QVector<double>& values, const QStringList& labels, int labelEvery = 4)
    {
        m_values = values;
        m_labels = labels;
        m_labelEvery = qMax(1, labelEvery);
        update();
    }

protected:
    void paintEvent(QPaintEvent*) override
    {
        QPainter p(this);
        p.setRenderHint(QPainter::Antialiasing);
        p.fillRect(rect(), Qt::white);

        QFont titleFont = font();
        titleFont.setPointSize(11);
        titleFont.setBold(true);
        p.setFont(titleFont);
        p.setPen(QColor("#333333"));
        p.drawText(QRect(0, 8, width(), 24), Qt::AlignHCenter | Qt::AlignVCenter, m_title);

        if (m_values.isEmpty()) {
            QFont f = font();
            f.setPointSize(10);
            p.setFont(f);
            p.setPen(QColor("#888888"));
            p.drawText(QRect(0, height() / 2 - 12, width(), 24), Qt::AlignCenter, "—");
            return;
        }

        double maxV = 0.0;
        for (double v : m_values) maxV = qMax(maxV, v);
        if (maxV <= 0.0) maxV = 1.0;

        int left = 14, right = 10, top = 40, bottom = 30;
        int chartW = width() - left - right;
        int chartH = height() - top - bottom;
        int n = m_values.size();
        double slotW = double(chartW) / n;
        double barW = slotW * 0.62;

        // Grid + max label
        QFont f = font();
        f.setPointSize(8);
        p.setFont(f);
        p.setPen(QPen(QColor("#dddddd"), 1));
        p.drawLine(left, top + chartH, left + chartW, top + chartH);
        p.setPen(QColor("#666666"));
        p.drawText(QRect(0, top - 16, left + 34, 16), Qt::AlignRight | Qt::AlignVCenter,
                   QString::number(int(maxV)));

        // Bars
        for (int i = 0; i < n; ++i) {
            double v = m_values[i];
            double h = v / maxV * (chartH - 6);
            double x = left + i * slotW + (slotW - barW) / 2.0;
            double y = top + chartH - h;
            bool isMax = v >= maxV - 0.0001 && v > 0.0;
            p.setPen(Qt::NoPen);
            p.setBrush(QColor(isMax ? "#e53935" : "#1e88e5"));
            p.drawRoundedRect(QRectF(x, y, barW, h), 3, 3);

            // Value on top
            if (v > 0.0) {
                p.setPen(QColor("#333333"));
                p.drawText(QRectF(x - 8, y - 14, barW + 16, 12),
                           Qt::AlignHCenter, QString::number(int(v)));
            }
            // Slotted label below
            if (i % m_labelEvery == 0 && i < m_labels.size()) {
                p.setPen(QColor("#666666"));
                QString lbl = m_labels[i];
                p.drawText(QRectF(x - 14, top + chartH + 4, barW + 28, 16),
                           Qt::AlignHCenter, lbl);
            }
        }
    }

private:
    QString m_title;
    QVector<double> m_values;
    QStringList m_labels;
    int m_labelEvery = 4;
};
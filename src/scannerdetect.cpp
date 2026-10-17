#include "scannerdetect.h"

#include <QFile>
#include <QString>
#include <QStringList>

#if defined(Q_OS_WIN)
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include <windows.h>
#include <vector>
#endif

namespace {

// Scanner devices frequently surface with recognizable names ("HID
// Barcode Scanner", "POS Keyboard", vendor/model tags, ...). Case- and
// prefix-agnostic keyword match.
bool nameLooksLikeScanner(const QString& name)
{
    if (name.isEmpty()) return false;
    const QString n = name.toLower();
    const QStringList keywords = {
        QStringLiteral("barcode"), QStringLiteral("bar code"), QStringLiteral("scanner"),
        QStringLiteral("wedge"),   QStringLiteral("pos keyboard"), QStringLiteral("honeywell"),
        QStringLiteral("zebra"),   QStringLiteral("unitech"),       QStringLiteral("datalogic"),
        QStringLiteral("symbol"),  QStringLiteral("motorola"),      QStringLiteral("opticon"),
    };
    for (const QString& k : keywords) {
        if (n.contains(k)) return true;
    }
    return false;
}

} // namespace

namespace ScannerProbe {

// Fresh hardware scan (never cached) — used by the "Rescan devices"
// action so the operator can re-detect after plugging a scanner in.
bool detectScannerNow()
{
#if defined(Q_OS_WIN)
    UINT n = 0;
    if (GetRawInputDeviceList(nullptr, &n, sizeof(RAWINPUTDEVICELIST))
            == static_cast<UINT>(-1)) {
        return false;
    }
    if (n == 0) return false;
    std::vector<RAWINPUTDEVICELIST> devices(n);
    UINT got = GetRawInputDeviceList(devices.data(), &n, sizeof(RAWINPUTDEVICELIST));
    int keyboardDevices = 0;
    for (UINT i = 0; i < got; ++i) {
        if (devices[i].dwType == RIM_TYPEKEYBOARD) {
            ++keyboardDevices;
            // Also check the device name: some scanner drivers name
            // their keyboard device explicitly.
            wchar_t buffer[512] = {0};
            UINT cb = sizeof(buffer);
            if (GetRawInputDeviceInfoW(devices[i].hDevice, RIDI_DEVICENAME,
                                       buffer, &cb) != static_cast<UINT>(-1)) {
                if (nameLooksLikeScanner(QString::fromWCharArray(buffer))) return true;
            }
        } else if (devices[i].dwType == RIM_TYPEHID) {
            wchar_t buffer[512] = {0};
            UINT cb = sizeof(buffer);
            if (GetRawInputDeviceInfoW(devices[i].hDevice, RIDI_DEVICENAME,
                                       buffer, &cb) != static_cast<UINT>(-1)) {
                if (nameLooksLikeScanner(QString::fromWCharArray(buffer))) return true;
            }
        }
    }
    return keyboardDevices >= 2;

#elif defined(Q_OS_LINUX)
    QFile file(QStringLiteral("/proc/bus/input/devices"));
    if (!file.open(QIODevice::ReadOnly | QIODevice::Text)) return false;
    const QString all = QString::fromUtf8(file.readAll());
    const QStringList blocks = all.split(QStringLiteral("\n\n"), Qt::SkipEmptyParts);
    int keyboards = 0;
    for (const QString& block : blocks) {
        QString name;
        const QStringList lines = block.split(QLatin1Char('\n'));
        for (const QString& line : lines) {
            if (line.startsWith(QStringLiteral("N: Name=")))
                name = line.mid(8).trimmed();
        }
        name.remove(QLatin1Char('"'));
        if (nameLooksLikeScanner(name)) return true;
        // Only count devices that are actually keyboards (by name,
        // not by "kbd" handler, which sensors also use).
        const QString lower = name.toLower();
        if (lower.contains(QStringLiteral("keyboard"))
            || lower.contains(QStringLiteral("keypad"))
            || (lower.contains(QStringLiteral("hid"))
                && lower.contains(QStringLiteral("keys")))) {
            ++keyboards;
        }
    }
    return keyboards >= 2;

#else
    // macOS/other: no reliable generic way to enumerate keyboard devices
    // without extra frameworks — default to "not found" (mode stays off
    // until toggled manually).
    return false;
#endif
}

bool hardwareScannerPresent()
{
    static const bool detected = detectScannerNow();
    return detected;
}

} // namespace ScannerProbe
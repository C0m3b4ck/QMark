#include "zipwriter.h"

#include <QDateTime>

namespace Zip {

// ── CRC-32 (IEEE 802.3) ────────────────────────────────────────────
namespace {
quint32 crcTableRow(int i)
{
    quint32 c = (quint32)i;
    for (int k = 0; k < 8; ++k) {
        c = (c & 1U) ? (0xEDB88320U ^ (c >> 1)) : (c >> 1);
    }
    return c;
}
} // namespace

quint32 crc32(const QByteArray& data)
{
    static quint32 table[256] = {};
    static bool inited = false;
    if (!inited) {
        for (int i = 0; i < 256; ++i) table[i] = crcTableRow(i);
        inited = true;
    }
    quint32 c = 0xFFFFFFFFU;
    const auto* p = reinterpret_cast<const unsigned char*>(data.constData());
    for (qsizetype i = 0; i < data.size(); ++i) {
        c = (c >> 8) ^ table[(c ^ p[i]) & 0xFFU];
    }
    return c ^ 0xFFFFFFFFU;
}

// ── ZIP writer (store-only) ────────────────────────────────────────
namespace {

struct Entry {
    QString name;
    QByteArray data;
    quint32 crc = 0;
};

// Fixed "timestamp" keeps archives deterministic: 2024-01-01 00:00:00
QByteArray dosDateTimeBytes()
{
    // DOS time: 0x0000; DOS date: 2024-01-01 → (year-1980)<<9 | month<<5 | day
    quint16 date = quint16((2024 - 1980) << 9) | quint16(1 << 5) | 1; // 0x4D21
    QByteArray b;
    b.append(char(0)); b.append(char(0));              // time (2 bytes LE)
    b.append(char(date & 0xFF)); b.append(char(date >> 8)); // date (2 bytes LE)
    return b;
}

void appendU16(QByteArray& b, quint16 v)
{
    b.append(char(v & 0xFF));
    b.append(char((v >> 8) & 0xFF));
}

void appendU32(QByteArray& b, quint32 v)
{
    b.append(char(v & 0xFF));
    b.append(char((v >> 8) & 0xFF));
    b.append(char((v >> 16) & 0xFF));
    b.append(char((v >> 24) & 0xFF));
}

} // namespace

QByteArray pack(const QList<QPair<QString, QByteArray>>& files)
{
    QList<Entry> entries;
    for (const auto& f : files) {
        Entry e;
        e.name = f.first;
        e.data = f.second;
        e.crc = crc32(f.second);
        entries.push_back(std::move(e));
    }

    QByteArray out;
    QList<quint32> centralOffsets;
    centralOffsets.reserve(entries.size());

    const QByteArray dosTime = dosDateTimeBytes();

    for (const auto& e : entries) {
        const QByteArray name = e.name.toUtf8();
        const quint32 nameLen = quint32(name.size());
        const quint32 dataLen = quint32(e.data.size());

        centralOffsets.push_back(quint32(out.size()));

        // Local file header
        out.append("PK\x03\x04");
        appendU16(out, 20);              // version needed
        appendU16(out, 0x0800);          // general purpose bit flag (UTF-8)
        appendU16(out, 0);               // compression: store
        out.append(dosTime);
        appendU32(out, e.crc);
        appendU32(out, dataLen);         // compressed size == size
        appendU32(out, dataLen);
        appendU16(out, nameLen);
        appendU16(out, 0);               // extra length
        out.append(name);
        out.append(e.data);
    }

    const quint32 centralStart = quint32(out.size());
    for (int i = 0; i < entries.size(); ++i) {
        const auto& e = entries[i];
        const QByteArray name = e.name.toUtf8();
        const quint32 dataLen = quint32(e.data.size());

        // Central directory header
        out.append("PK\x01\x02");
        appendU16(out, 20);              // version made by
        appendU16(out, 20);              // version needed
        appendU16(out, 0x0800);          // UTF-8 names
        appendU16(out, 0);               // store
        out.append(dosTime);
        appendU32(out, e.crc);
        appendU32(out, dataLen);
        appendU32(out, dataLen);
        appendU16(out, quint16(name.size()));
        appendU16(out, 0);               // extra
        appendU16(out, 0);               // comment
        appendU16(out, 0);               // disk number
        appendU16(out, 0);               // internal attrs
        appendU32(out, 0);               // external attrs
        appendU32(out, centralOffsets[i]);
        out.append(name);
    }

    const quint32 centralSize = quint32(out.size()) - centralStart;
    const quint32 entryCount = quint32(entries.size());

    // End of central directory
    out.append("PK\x05\x06");
    appendU16(out, 0);
    appendU16(out, 0);
    appendU16(out, quint16(entryCount));
    appendU16(out, quint16(entryCount));
    appendU32(out, centralSize);
    appendU32(out, centralStart);
    appendU16(out, 0);                   // comment length

    return out;
}

} // namespace Zip
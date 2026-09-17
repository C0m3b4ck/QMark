#include <QString>

QString sanitize_string(QString stringValue)
{
    stringValue.replace("\\", "\\\\");
    stringValue.replace("'", "\\'");
    stringValue.replace("\"", "\\\"");
    stringValue.replace('\n', "");
    stringValue.replace('\r', "");
    stringValue.replace('\0', "");
    stringValue.replace(QChar(0x1A), "");
    return stringValue;
}

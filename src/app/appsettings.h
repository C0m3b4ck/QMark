#pragma once

// ─────────────────────────────────────────────────────────────────────
// appsettings.h — central registry of persistent settings keys used by
// the automation/remote features (scanner, e-mail, backups, auto-update,
// remote dashboard). All keys live under the "QMark"/"SchoolShop"
// organization/application pair used everywhere else.
//
// Load convenience: every getter applies its documented default, so new
// installations never crash on missing keys. Values are plain QSettings
// entries; anything sensitive (SMTP password, remote token) is stored as
// plain text in the registry/inifile — noted in the UI.
// ─────────────────────────────────────────────────────────────────────

#include <QSettings>
#include <QString>
#include <QStringList>

namespace AppSettings {

// ── Key strings ────────────────────────────────────────────────────
inline QString keyRemoteEnabled()  { return "remote/enabled"; }
inline QString keyRemotePort()     { return "remote/port"; }
inline QString keyRemoteToken()    { return "remote/token"; }

inline QString keyScanEnabled()    { return "scan/enabled"; }

inline QString keyMailEnabled()    { return "mail/enabled"; }
inline QString keyMailHost()       { return "mail/host"; }
inline QString keyMailPort()       { return "mail/port"; }
// 0 = STARTTLS (587, autonegotiated), 1 = implicit TLS (465), 2 = plaintext
inline QString keyMailSecurity()   { return "mail/security"; }
inline QString keyMailUsername()   { return "mail/username"; }
inline QString keyMailPassword()   { return "mail/password"; }
inline QString keyMailFrom()       { return "mail/from"; }
inline QString keyMailRecipients() { return "mail/recipients"; }
inline QString keyMailDailyEnabled() { return "mail/dailyEnabled"; }
inline QString keyMailDailyTime()  { return "mail/dailyTime"; }        // "HH:MM"
inline QString keyMailDailyLast()  { return "mail/dailyLast"; }        // "yyyy-MM-dd"
inline QString keyMailMonthlyEnabled() { return "mail/monthlyEnabled"; }
inline QString keyMailMonthlyDay() { return "mail/monthlyDay"; }        // 1..28
inline QString keyMailMonthlyTime(){ return "mail/monthlyTime"; }
inline QString keyMailMonthlyLast(){ return "mail/monthlyLast"; }       // "yyyy-MM"
inline QString keyMailAttachBackup(){ return "mail/attachBackup"; }

inline QString keyBackupFolder()   { return "backup/folder"; }
inline QString keyBackupKeep()     { return "backup/keep"; }
inline QString keyBackupScheduleEnabled() { return "backup/scheduleEnabled"; }
inline QString keyBackupScheduleTime()    { return "backup/scheduleTime"; }
inline QString keyBackupScheduleLast()    { return "backup/scheduleLast"; }
inline QString keyBackupOnlineMail(){ return "backup/onlineMail"; }     // off by default
inline QString keyBackupOnlineHttp(){ return "backup/onlineHttp"; }     // off by default
inline QString keyBackupHttpUrl()  { return "backup/httpUrl"; }
inline QString keyBackupHttpToken(){ return "backup/httpToken"; }
inline QString keyBackupLastAt()   { return "backup/lastAt"; }

inline QString keyUpdateEnabled()  { return "update/enabled"; }
inline QString keyUpdateLastCheck(){ return "update/lastCheck"; }
inline QString keyUpdateLastSeen() { return "update/lastSeen"; }

// ── Typed getters (with defaults) ───────────────────────────────────
// Shared application handle (org "QMark", app "SchoolShop") — the same
// pairing used by every other module in the app.
inline QSettings& settings()
{
    static QSettings instance("QMark", "SchoolShop");
    return instance;
}

inline bool remoteEnabled()            { return settings().value(keyRemoteEnabled(), false).toBool(); }
inline int  remotePort()               { return settings().value(keyRemotePort(), 8080).toInt(); }
inline QString remoteToken()           { return settings().value(keyRemoteToken()).toString(); }

inline bool scanEnabled()              { return settings().value(keyScanEnabled(), true).toBool(); }

inline bool mailEnabled()              { return settings().value(keyMailEnabled(), false).toBool(); }
inline QString mailHost()              { return settings().value(keyMailHost()).toString(); }
inline int  mailPort()                 { return settings().value(keyMailPort(), 587).toInt(); }
inline int  mailSecurity()             { return settings().value(keyMailSecurity(), 0).toInt(); } // 0 starttls, 1 implicit, 2 plain
inline QString mailUsername()          { return settings().value(keyMailUsername()).toString(); }
inline QString mailPassword()          { return settings().value(keyMailPassword()).toString(); }
inline QString mailFrom()              { return settings().value(keyMailFrom()).toString(); }
inline QStringList mailRecipients() {
    return settings().value(keyMailRecipients()).toStringList();
}
inline bool mailDailyEnabled()         { return settings().value(keyMailDailyEnabled(), false).toBool(); }
inline QString mailDailyTime()         { return settings().value(keyMailDailyTime(), "19:00").toString(); }
inline QString mailDailyLast()         { return settings().value(keyMailDailyLast()).toString(); }
inline bool mailMonthlyEnabled()       { return settings().value(keyMailMonthlyEnabled(), false).toBool(); }
inline int  mailMonthlyDay()           { return settings().value(keyMailMonthlyDay(), 1).toInt(); }
inline QString mailMonthlyTime()       { return settings().value(keyMailMonthlyTime(), "08:00").toString(); }
inline QString mailMonthlyLast()       { return settings().value(keyMailMonthlyLast()).toString(); }
inline bool mailAttachBackup()         { return settings().value(keyMailAttachBackup(), true).toBool(); }

inline QString backupFolder()          { return settings().value(keyBackupFolder(), "backups").toString(); }
inline int  backupKeep()               { return qMax(1, settings().value(keyBackupKeep(), 10).toInt()); }
inline bool backupScheduleEnabled()    { return settings().value(keyBackupScheduleEnabled(), false).toBool(); }
inline QString backupScheduleTime()    { return settings().value(keyBackupScheduleTime(), "20:00").toString(); }
inline QString backupScheduleLast()    { return settings().value(keyBackupScheduleLast()).toString(); }
inline bool backupOnlineMail()         { return settings().value(keyBackupOnlineMail(), false).toBool(); }
inline bool backupOnlineHttp()         { return settings().value(keyBackupOnlineHttp(), false).toBool(); }
inline QString backupHttpUrl()         { return settings().value(keyBackupHttpUrl()).toString(); }
inline QString backupHttpToken()       { return settings().value(keyBackupHttpToken()).toString(); }
inline QString backupLastAt()          { return settings().value(keyBackupLastAt()).toString(); }

inline bool updateEnabled()            { return settings().value(keyUpdateEnabled(), false).toBool(); }
inline QString updateLastCheck()       { return settings().value(keyUpdateLastCheck()).toString(); }
inline QString updateLastSeen()        { return settings().value(keyUpdateLastSeen()).toString(); }

} // namespace AppSettings
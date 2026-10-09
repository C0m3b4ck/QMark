#ifndef MAINWINDOW_H
#define MAINWINDOW_H

#include <QMainWindow>
#include <QListWidget>
#include <QLabel>
#include <QGridLayout>
#include <QScrollArea>
#include <QVector>
#include <QElapsedTimer>
#include <optional>
#include "domain.h"
#include "businesslogic.h"
#include "dataaccess.h"
#include "worklog.h"

class QTimer;
class QCheckBox;
class QComboBox;
class QSpinBox;
class QLineEdit;
class QPlainTextEdit;
class QTimeEdit;
class RemoteServer;

QT_BEGIN_NAMESPACE
namespace Ui {
class MainWindow;
}
QT_END_NAMESPACE

class MainWindow : public QMainWindow
{
    Q_OBJECT

public:
    explicit MainWindow(DataAccess::IDataAccess& db, QWidget *parent = nullptr);
    ~MainWindow() override;

protected:
    void closeEvent(QCloseEvent *event) override;
    void resizeEvent(QResizeEvent *event) override;
    void keyPressEvent(QKeyEvent *event) override;
    bool eventFilter(QObject *watched, QEvent *event) override;

public:
    bool isLoggedIn() const;
    void setLoggedIn(bool loggedIn);
    bool checkLoginRequired(bool requireDatabase = true);
    bool checkRoleRequired(BusinessLogic::RequiredRole required, bool requireDatabase = true);
    Domain::User::Role getCurrentUserRole() const;
    void setCurrentUser(const Domain::User& user);
    void clearCurrentUser();
    void syncLanguageUi();

private slots:
    // ── Login / Register ──────────────────────────────────────────
    void on_btnLogin_clicked();
    void on_btnRegister_clicked();
    void on_btnClear_username_register_clicked();
    void on_btnClear_password1_register_clicked();
    void on_btnClear_password2_register_clicked();
    void on_chkHide_login_toggled(bool checked);
    void on_chkHide_register_toggled(bool checked);
    void on_txtPassword1_register_textChanged(const QString &text);
    void on_btnHelp_role_register_clicked();

    // ── Menu ──────────────────────────────────────────────────────
    void on_actionClose_triggered();
    void on_actionLog_out_triggered();
    void on_actionLog_in_triggered();
    void on_actionRegister_triggered();
    void on_actionLanguageEnglish_triggered();
    void on_actionLanguagePolish_triggered();

    // ── Items ─────────────────────────────────────────────────────
    void on_actionAdd_Items_triggered();
    void on_actionEdit_Items_triggered();
    void on_actionRemove_Items_triggered();

    // Add Item page
    void on_btnAdd_item_clicked();
    void on_btnClear_name_item_clicked();
    void on_btnClear_id_item_clicked();
    void on_btnCheckId_item_clicked();
    void on_chkAutogenerateID_item_toggled(bool checked);
    void on_txtId_item_textChanged(const QString &text);

    // Edit Item page
    void on_btnEdit_item_clicked();
    void on_btnSearch_item_edit_clicked();
    void on_btnClear_name_item_edit_clicked();
    void on_btnClear_id_item_edit_clicked();
    void on_lstSearch_item_edit_itemClicked(QListWidgetItem *item);

    // Remove Item page
    void on_btnRemove_item_clicked();
    void on_btnSearch_item_remove_clicked();
    void on_btnUndoRemove_item_clicked();
    void on_btnUndoLast_item_clicked();
    void on_lstSearch_item_remove_itemClicked(QListWidgetItem *item);

    // ── Sell Item (POS) ──────────────────────────────────────────
    void on_actionSell_Item_triggered();
    void on_btnSearch_sell_page_clicked();
    void on_chkKeybinds_sell_toggled(bool checked);
    void on_chkSimpleView_sell_toggled(bool checked);
    void on_btnRefreshSalesLog_sell_clicked();
    void on_btnUndoSale_sell_clicked();

    // ── Resupply ──────────────────────────────────────────────────
    void on_actionResupply_triggered();

    // ── Categories ────────────────────────────────────────────────
    void on_actionManage_Categories_triggered();
    void on_btnAdd_category_clicked();
    void on_btnEdit_category_clicked();
    void on_btnRemove_category_clicked();
    void on_btnClear_name_category_clicked();
    void on_btnUndoAdd_category_clicked();
    void on_btnUndoEdit_category_clicked();
    void on_btnUndoRemove_category_clicked();
    void on_lstSearch_category_itemClicked(QListWidgetItem *item);

    // ── Shelves ───────────────────────────────────────────────────
    void on_actionManage_Shelves_triggered();
    void on_btnAdd_shelf_clicked();
    void on_btnEdit_shelf_clicked();
    void on_btnRemove_shelf_clicked();
    void on_btnClear_name_shelf_clicked();
    void on_btnUndoAdd_shelf_clicked();
    void on_btnUndoEdit_shelf_clicked();
    void on_btnUndoRemove_shelf_clicked();
    void on_lstSearch_shelf_itemClicked(QListWidgetItem *item);

    // ── Sales History ─────────────────────────────────────────────
    void on_actionSales_History_triggered();
    void on_btnSearch_sales_clicked();
    void on_btnRefresh_sales_clicked();
    void on_btnExport_sales_clicked();
    void on_chkSimpleView_sales_toggled(bool checked);

    // ── Undo ──────────────────────────────────────────────────────
    void on_actionUndo_Removed_Items_triggered();
    void on_btnUndoAll_undoremoved_clicked();
    void on_btnUndoSelected_undoremoved_clicked();
    void on_lstSearch_undoremoved_itemClicked(QListWidgetItem *item);
    void refreshRemovedItemsList();

    // ── Reports ───────────────────────────────────────────────────
    void on_actionMake_Report_triggered();
    void on_btnGenerateReport_clicked();
    void on_btnExportReport_clicked();
    void on_btnExportPdfReport_clicked();
    void on_btnExitReport_clicked();

    // ── Database ──────────────────────────────────────────────────
    void on_actionDatabase_Selection_triggered();
    void on_actionCreate_Database_triggered();
    void on_btnLoadDbConfig_clicked();
    void on_btnBrowseItemsDb_clicked();
    void on_btnBrowseUsersDb_clicked();
    void on_btnSaveAsDefault_clicked();
    void on_btnTestConnection_clicked();
    void on_btnCreateNewDb_clicked();
    void on_chkTelemetry_toggled(bool checked);

    // ── Accounts ──────────────────────────────────────────────────
    void on_actionAccounts_triggered();
    void on_btnSearchAccount_clicked();
    void on_btnChangeRole_clicked();
    void on_btnChangePassword_clicked();
    void on_btnDeleteAccount_clicked();

    // ── Preferences ───────────────────────────────────────────────
    void on_actionPreferences_triggered();
    void on_btnSavePreferences_clicked();
    void on_btnResetPreferences_clicked();
    void on_btnConvertPrices_pref_clicked();
    void on_chkWorklog_toggled(bool checked);

    // ── Automation & Remote ───────────────────────────────────────
    void on_actionAutomation_triggered();
    void onAutomationSaveClicked();

    // ── Worklog Stats ─────────────────────────────────────────────
    void on_actionWorklogStats_triggered();
    void on_btnRefreshWorklog_clicked();
    void on_btnExportWorklog_clicked();

    // ── Dashboard ──────────────────────────────────────────────────
    void on_btnDashboardSell_clicked();
    void on_lstRecentSales_itemClicked(QListWidgetItem *item);

    // ── Troubleshoot ──────────────────────────────────────────────
    void on_actionTroubleshoot_triggered();
    void on_btnTestDbConnection_clicked();
    void on_btnViewLogs_clicked();
    void on_btnExportDiagnostics_clicked();

private:
    Ui::MainWindow *ui;
    DataAccess::IDataAccess& m_db;
    bool m_isLoggedIn = false;
    std::optional<Domain::User> m_currentUser;
    bool m_loadingDbConfigs = false;
    Worklog m_worklog;
    QString m_worklogFilePath;

    // ── Page navigation / roles ───────────────────────────────────
    void goToPage(int index);
    void applyRoleRestrictions();

    // ── Language / currency ───────────────────────────────────────
    void applyLanguageToUi();
    void updatePricePlaceholders();

    // ── First-run setup ───────────────────────────────────────────
    QWidget* buildFirstRunPage();
    QWidget* m_firstRunPage = nullptr;

    // ── Resupply ─────────────────────────────────────────────────
    QWidget* buildResupplyPage();
    QWidget* m_resupplyPage = nullptr;

    // ── Dashboard ────────────────────────────────────────────────
    void refreshDashboard();
    void updateDashboardClock();

    // ── POS Grid helpers ──────────────────────────────────────────
    void rebuildItemGrid(const std::vector<Domain::Item>& items);
    QWidget* createItemCard(const Domain::Item& item, const QString& keyHint = QString());
    void refreshSellPage();
    void refreshSalesLog();
    void refreshSellStats();
    void highlightSellCard(int index);
    bool sellProductAtIndex(int index);
    void undoSaleById(const QString& saleId);
    QString m_selectedSellItemId;

    // ── Sales History helpers ─────────────────────────────────────
    void populateSalesList(const std::vector<Domain::Sale>& sales);

    // Keybinds state (sell page)
    bool m_keybindsEnabled = true;
    std::vector<Domain::Item> m_sellItems;   // items currently shown in the grid
    QVector<QWidget*> m_sellCards;           // card widgets in grid order
    int m_sellHighlightIndex = -1;           // currently highlighted card (-1 = none)

    // ── Dashboard clock (updates every second) ────────────────────
    QTimer *m_clockTimer = nullptr;

    // ── Automation (scanner / e-mail / backups / update / remote) ─
    QWidget* buildAutomationPage();
    QWidget* m_automationPage = nullptr;
    void applyAutomationPageSettings();
    void saveAutomationPageSettings();
    void startRemoteServerIfEnabled();
    void stopRemoteServer();
    void onSchedulerTick();
    void sendSummaryEmailNow(bool monthly);
    void createBackupNow();
    void checkUpdatesNow();
    void handleScannedCode(const QString& code);
    void startAddWithBarcode(const QString& code);
    void loadItemToEdit(const QString& id);
    void setScannerMode(bool on);
    bool sellItemById(const std::string& itemId);
    void showStatus(const QString& msg, int ms = 7000);
    void noteUpdateCheckResult(const QString& result, const QString& detail);

    // Widget pointers for the programmatically built Automation page,
    // so Tr::applyLanguage() + save/load can reach them by name.
    struct AutomationUi {
        // Scanner mode
        QCheckBox* chkScanner = nullptr;
        // Remote dashboard
        QCheckBox* chkRemote = nullptr;
        QSpinBox*  spinRemotePort = nullptr;
        QLineEdit* txtRemoteToken = nullptr;
        QLabel*    lblRemoteStatus = nullptr;
        // E-mail summary
        QCheckBox*     chkMail = nullptr;
        QLineEdit*     txtMailHost = nullptr;
        QSpinBox*      spinMailPort = nullptr;
        QComboBox*     cboMailSecurity = nullptr;
        QLineEdit*     txtMailUser = nullptr;
        QLineEdit*     txtMailPass = nullptr;
        QLineEdit*     txtMailFrom = nullptr;
        QPlainTextEdit* txtMailRecipients = nullptr;
        QCheckBox*     chkMailDaily = nullptr;
        QTimeEdit*     timeMailDaily = nullptr;
        QCheckBox*     chkMailMonthly = nullptr;
        QSpinBox*      spinMailMonthlyDay = nullptr;
        QTimeEdit*     timeMailMonthly = nullptr;
        QCheckBox*     chkMailAttach = nullptr;
        QLabel*        lblMailStatus = nullptr;
        // Backups
        QLineEdit* txtBackupFolder = nullptr;
        QSpinBox*  spinBackupKeep = nullptr;
        QCheckBox* chkBackupSchedule = nullptr;
        QTimeEdit* timeBackupSchedule = nullptr;
        QCheckBox* chkBackupOnlineMail = nullptr;
        QCheckBox* chkBackupOnlineHttp = nullptr;
        QLineEdit* txtBackupHttpUrl = nullptr;
        QLineEdit* txtBackupHttpToken = nullptr;
        QLabel*    lblBackupStatus = nullptr;
        // Auto-update
        QCheckBox* chkUpdates = nullptr;
        QLabel*    lblUpdateStatus = nullptr;
    };
    AutomationUi m_autoUi;

    RemoteServer* m_remoteServer = nullptr;
    QTimer* m_schedulerTimer = nullptr;
    bool m_mailInProgress = false;
    bool m_backupInProgress = false;
    bool m_updateCheckInProgress = false;
    // True while applyAutomationPageSettings() populates the widgets; the
    // toggled() handlers must not persist half-filled state during that.
    bool m_applyingAutomationSettings = false;

    // ── Scanner mode state ─────────────────────────────────────────
    bool m_scannerEnabled = false;
    bool m_scannerHasCustomList = false;   // reserved for future use
    QElapsedTimer m_scannerKeyTimer;       // burst gap measurement
    QString m_scannerBuffer;               // chars collected in the burst
    qint64 m_scannerLastNs = 0;            // nsecs of the last keypress
    qint64 m_scannerGapNs = 0;             // gap to the previous keypress
    bool m_scannerInBurst = false;         // a scanning burst is active
};

#endif // MAINWINDOW_H
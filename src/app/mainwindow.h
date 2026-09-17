#ifndef MAINWINDOW_H
#define MAINWINDOW_H

#include <QMainWindow>
#include <QListWidget>
#include <QLabel>
#include <QGridLayout>
#include <QScrollArea>
#include <optional>
#include "domain.h"
#include "businesslogic.h"
#include "dataaccess.h"
#include "worklog.h"

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

public:
    bool isLoggedIn() const;
    void setLoggedIn(bool loggedIn);
    bool checkLoginRequired(bool requireDatabase = true);
    bool checkRoleRequired(BusinessLogic::RequiredRole required, bool requireDatabase = true);
    Domain::User::Role getCurrentUserRole() const;
    void setCurrentUser(const Domain::User& user);
    void clearCurrentUser();

private slots:
    // ── Login / Register ──────────────────────────────────────────
    void on_btnLogin_clicked();
    void on_btnRegister_clicked();
    void on_btnClear_username_register_clicked();
    void on_btnClear_password1_register_clicked();
    void on_btnClear_password2_register_clicked();
    void on_chkHide_login_toggled(bool checked);
    void on_chkHide_register_toggled(bool checked);
    void on_txtPwd1_register_textChanged(const QString &text);

    // ── Menu ──────────────────────────────────────────────────────
    void on_actionClose_triggered();
    void on_actionLog_out_triggered();
    void on_actionLog_in_triggered();
    void on_actionRegister_triggered();

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
    void on_btnSearch_sell_clicked();
    void on_btnSellItem_clicked();
    void on_lstSearch_sell_itemClicked(QListWidgetItem *item);

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

    // ── Undo ──────────────────────────────────────────────────────
    void on_actionUndo_Removed_Items_triggered();
    void on_btnUndoAll_undoremoved_clicked();
    void on_btnUndoSelected_undoremoved_clicked();
    void on_lstSearch_undoremoved_itemClicked(QListWidgetItem *item);

    // ── Reports ───────────────────────────────────────────────────
    void on_actionMake_Report_triggered();
    void on_btnGenerateReport_clicked();
    void on_btnExportReport_clicked();

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
    void on_chkWorklog_toggled(bool checked);

    // ── Worklog Stats ─────────────────────────────────────────────
    void on_actionWorklogStats_triggered();
    void on_btnRefreshWorklog_clicked();
    void on_btnExportWorklog_clicked();

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

    // ── POS Grid helpers ──────────────────────────────────────────
    void rebuildItemGrid(const std::vector<Domain::Item>& items);
    QWidget* createItemCard(const Domain::Item& item);
    QString m_selectedSellItemId;
};

#endif // MAINWINDOW_H

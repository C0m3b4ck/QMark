#include "mainwindow.h"
#include "ui_mainwindow.h"
#include "sqlite_dataaccess.h"
#include "businesslogic.h"
#include "crypto.h"
#include "logger.h"
#include "telemetry.h"
#include <QMessageBox>
#include <QFileDialog>
#include <QInputDialog>
#include <QCloseEvent>
#include <QResizeEvent>
#include <QScrollBar>
#include <QGridLayout>
#include <QScrollArea>
#include <QFrame>
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QPushButton>
#include <QLabel>
#include <QFont>
#include <QSettings>
#include <QDateTime>
#include <QDir>
#include <QFile>
#include <QTextStream>
#include <QPointer>
#include <sstream>
#include <iomanip>

MainWindow::MainWindow(DataAccess::IDataAccess& db, QWidget *parent)
    : QMainWindow(parent)
    , ui(new Ui::MainWindow)
    , m_db(db)
{
    ui->setupUi(this);

    // ── Window title ──────────────────────────────────────────
    setWindowTitle("QMark — School Shop PoS");

    // ── Load preferences ──────────────────────────────────────
    QSettings settings("QMark", "SchoolShop");
    bool telemetryEnabled = settings.value("telemetry/enabled", false).toBool();
    ui->chkTelemetry->setChecked(telemetryEnabled);
    if (telemetryEnabled) {
        QString logPath = QDir::currentPath() + "/telemetry_"
            + QDateTime::currentDateTime().toString("yyyyMMdd_hhmmss") + ".log";
        AppLogger::instance().setTelemetryEnabled(true);
        AppLogger::instance().setLogFile(logPath);
    }

    bool worklogEnabled = settings.value("worklog/enabled", false).toBool();
    ui->chkWorklog->setChecked(worklogEnabled);
    m_worklog.setEnabled(worklogEnabled);

    // Connect Sell button in top bar
    connect(ui->btnSellNav, &QPushButton::clicked, this, &MainWindow::on_actionSell_Item_triggered);

    // ── Start at login page ───────────────────────────────────
    if (ui->stackedWidget) {
        ui->stackedWidget->setCurrentIndex(0); // login page
    }
}

MainWindow::~MainWindow()
{
    delete ui;
}

void MainWindow::closeEvent(QCloseEvent *event)
{
    m_worklog.close();
    AppLogger::instance().close();
    event->accept();
}

void MainWindow::resizeEvent(QResizeEvent *event)
{
    QMainWindow::resizeEvent(event);
    // Auto-adapt grid columns when window resizes by rebuilding the grid
    // (only if items are currently displayed on the POS page)
}

// ═══════════════════════════════════════════════════════════════════
// Login / Register
// ═══════════════════════════════════════════════════════════════════

bool MainWindow::isLoggedIn() const { return m_isLoggedIn; }
void MainWindow::setLoggedIn(bool loggedIn) { m_isLoggedIn = loggedIn; }

bool MainWindow::checkLoginRequired(bool)
{
    if (!m_isLoggedIn) {
        QMessageBox::information(this, "Login Required", "Please log in first.");
        return false;
    }
    return true;
}

bool MainWindow::checkRoleRequired(BusinessLogic::RequiredRole required, bool)
{
    if (!m_isLoggedIn) {
        QMessageBox::information(this, "Login Required", "Please log in first.");
        return false;
    }
    auto check = BusinessLogic::checkUserRole(m_currentUser, required);
    if (!check.hasAccess) {
        QMessageBox::warning(this, "Access Denied", QString::fromStdString(check.errorMessage));
        return false;
    }
    return true;
}

Domain::User::Role MainWindow::getCurrentUserRole() const
{
    if (m_currentUser.has_value()) return m_currentUser->role;
    return Domain::User::Role::UserRole;
}

void MainWindow::setCurrentUser(const Domain::User& user) { m_currentUser = user; }
void MainWindow::clearCurrentUser() { m_currentUser.reset(); m_isLoggedIn = false; }

// ── Login ──────────────────────────────────────────────────────
void MainWindow::on_btnLogin_clicked()
{
    QString username = ui->txtUsername_login->text().trimmed();
    QString password = ui->txtPassword_login->text();

    if (username.isEmpty() || password.isEmpty()) {
        QMessageBox::warning(this, "Login", "Please enter username and password.");
        return;
    }

    std::optional<Domain::User> user = BusinessLogic::login(
        m_db, username.toStdString(), password.toStdString());

    if (!user.has_value()) {
        QMessageBox::warning(this, "Login", "Invalid username or password.");
        return;
    }

    setCurrentUser(user.value());
    setLoggedIn(true);

    LOG_INFO("User logged in: " + username);

    // Navigate to main/sell page
    if (ui->stackedWidget) {
        ui->stackedWidget->setCurrentIndex(1); // main page
    }

    statusBar()->showMessage("Logged in as " + username + " (" + QString::fromStdString(user->roleName()) + ")");
}

// ── Register ───────────────────────────────────────────────────
void MainWindow::on_btnRegister_clicked()
{
    // Only SuperAdmin can register
    if (!m_isLoggedIn || getCurrentUserRole() != Domain::User::Role::SuperAdmin) {
        QMessageBox::warning(this, "Register", "Only SuperAdmin can register new users.");
        return;
    }

    QString username = ui->txtUsername_register->text().trimmed();
    QString password1 = ui->txtPassword1_register->text();
    QString password2 = ui->txtPassword2_register->text();

    if (password1 != password2) {
        QMessageBox::warning(this, "Register", "Passwords do not match.");
        return;
    }

    // Determine role from combo
    Domain::User::Role role = Domain::User::Role::UserRole;
    if (ui->cboRole_register) {
        int idx = ui->cboRole_register->currentIndex();
        if (idx == 2) role = Domain::User::Role::SuperAdmin;
        else if (idx == 1) role = Domain::User::Role::Admin;
    }

    DTO::UserDTO userDto;
    userDto.id = std::to_string(std::chrono::duration_cast<std::chrono::milliseconds>(
        std::chrono::system_clock::now().time_since_epoch()).count());
    userDto.username = username.toStdString();
    userDto.password = hash_string(password1.toStdString());
    if (userDto.password.empty()) {
        QMessageBox::warning(this, "Register", "Failed to hash password.");
        return;
    }
    userDto.role = role;

    auto result = BusinessLogic::addUser(m_db, userDto);
    if (result.isValid) {
        QMessageBox::information(this, "Register", "User registered successfully.");
        LOG_INFO("User registered: " + username);
    } else {
        QMessageBox::warning(this, "Register", QString::fromStdString(result.errorMessage));
    }
}

void MainWindow::on_btnClear_username_register_clicked() { ui->txtUsername_register->clear(); }
void MainWindow::on_btnClear_password1_register_clicked() { ui->txtPassword1_register->clear(); }
void MainWindow::on_btnClear_password2_register_clicked() { ui->txtPassword2_register->clear(); }

void MainWindow::on_chkHide_login_toggled(bool checked)
{
    ui->txtPassword_login->setEchoMode(checked ? QLineEdit::PasswordEchoOnEdit : QLineEdit::Normal);
}

void MainWindow::on_chkHide_register_toggled(bool checked)
{
    ui->txtPassword1_register->setEchoMode(checked ? QLineEdit::PasswordEchoOnEdit : QLineEdit::Normal);
    ui->txtPassword2_register->setEchoMode(checked ? QLineEdit::PasswordEchoOnEdit : QLineEdit::Normal);
}

void MainWindow::on_txtPwd1_register_textChanged(const QString &text)
{
    Q_UNUSED(text);
    // Could add password strength indicator here
}

// ═══════════════════════════════════════════════════════════════════
// Menu Actions
// ═══════════════════════════════════════════════════════════════════

void MainWindow::on_actionClose_triggered() { close(); }

void MainWindow::on_actionLog_out_triggered()
{
    LOG_INFO("User logged out: " + QString::fromStdString(m_currentUser.value_or(Domain::User{}).username));
    clearCurrentUser();
    if (ui->stackedWidget) {
        ui->stackedWidget->setCurrentIndex(0);
    }
    statusBar()->showMessage("Logged out");
}

void MainWindow::on_actionLog_in_triggered()
{
    if (ui->stackedWidget) {
        ui->stackedWidget->setCurrentIndex(0);
    }
}

void MainWindow::on_actionRegister_triggered()
{
    if (checkRoleRequired(BusinessLogic::RequiredRole::SuperAdmin)) {
        if (ui->stackedWidget) {
            // Could navigate to a dedicated register page
            QMessageBox::information(this, "Register", "Use the Register form to create a new user.");
        }
    }
}

// ═══════════════════════════════════════════════════════════════════
// Items — Add
// ═══════════════════════════════════════════════════════════════════

void MainWindow::on_actionAdd_Items_triggered()
{
    if (!checkLoginRequired()) return;
    if (ui->stackedWidget) ui->stackedWidget->setCurrentIndex(2); // Add Item page
}

void MainWindow::on_btnAdd_item_clicked()
{
    if (!checkRoleRequired(BusinessLogic::RequiredRole::Admin)) return;

    QString name = ui->txtName_item->text().trimmed();
    QString quantity = ui->txtQuantity_item->text().trimmed();
    QString price = ui->txtPrice_item->text().trimmed();
    QString category = ui->txtCategory_item->text().trimmed();
    QString shelf = ui->txtShelf_item->text().trimmed();
    QString status = ui->cboStatus_item ? ui->cboStatus_item->currentText() : "In Stock";
    QString id = ui->txtId_item->text().trimmed();

    if (ui->chkAutogenerateID_item && ui->chkAutogenerateID_item->isChecked()) {
        id = QString::number(std::chrono::duration_cast<std::chrono::milliseconds>(
            std::chrono::system_clock::now().time_since_epoch()).count());
    }

    DTO::ItemDTO itemDto;
    itemDto.id = id.toStdString();
    itemDto.name = name.toStdString();
    itemDto.quantity = quantity.toInt();
    itemDto.price = price.toDouble();
    itemDto.category = category.toStdString();
    itemDto.shelf = shelf.toStdString();
    itemDto.status = status.toStdString();
    itemDto.createdAt = Domain::toISOString(Domain::now());
    itemDto.updatedAt = Domain::toISOString(Domain::now());

    auto result = BusinessLogic::addItem(m_db, itemDto);
    if (result.isValid) {
        QMessageBox::information(this, "Add Item", "Item added successfully.");
        LOG_INFO("Item added: " + name);
        m_worklog.logEntry(WorklogEntry::ActionType::Add, WorklogEntry::EntityType::Item,
                          id.toStdString(), "Added item: " + name.toStdString());
        // Clear form
        ui->txtName_item->clear();
        ui->txtQuantity_item->clear();
        ui->txtPrice_item->clear();
        ui->txtCategory_item->clear();
        ui->txtShelf_item->clear();
        ui->txtId_item->clear();
    } else {
        QMessageBox::warning(this, "Add Item", QString::fromStdString(result.errorMessage));
    }
}

void MainWindow::on_btnClear_name_item_clicked() { ui->txtName_item->clear(); }
void MainWindow::on_btnClear_id_item_clicked() { ui->txtId_item->clear(); }

void MainWindow::on_chkAutogenerateID_item_toggled(bool checked)
{
    ui->txtId_item->setEnabled(!checked);
    if (checked) {
        ui->txtId_item->setText(QString::number(std::chrono::duration_cast<std::chrono::milliseconds>(
            std::chrono::system_clock::now().time_since_epoch()).count()));
    }
}

void MainWindow::on_btnCheckId_item_clicked()
{
    QString id = ui->txtId_item->text().trimmed();
    if (id.isEmpty()) {
        QMessageBox::information(this, "Check ID", "Enter an ID to check.");
        return;
    }
    bool exists = m_db.checkIdExists("items", id.toStdString());
    QMessageBox::information(this, "Check ID", exists ? "ID already exists." : "ID is available.");
}

void MainWindow::on_txtId_item_textChanged(const QString &text)
{
    Q_UNUSED(text);
}

// ═══════════════════════════════════════════════════════════════════
// Items — Edit
// ═══════════════════════════════════════════════════════════════════

void MainWindow::on_actionEdit_Items_triggered()
{
    if (!checkLoginRequired()) return;
    if (ui->stackedWidget) ui->stackedWidget->setCurrentIndex(3); // Edit Item page
}

void MainWindow::on_btnSearch_item_edit_clicked()
{
    QString term = ui->txtSearch_item_edit->text().trimmed();
    QString field = ui->cboSearchField_item_edit ? ui->cboSearchField_item_edit->currentText().toLower() : "";
    auto items = BusinessLogic::populateList(m_db, "items", term.toStdString(), field.toStdString());
    ui->lstSearch_item_edit->clear();
    for (const auto& item : items) {
        QListWidgetItem* lwi = new QListWidgetItem(QString::fromStdString(item.displayText));
        lwi->setData(Qt::UserRole, QString::fromStdString(item.id));
        ui->lstSearch_item_edit->addItem(lwi);
    }
}

void MainWindow::on_lstSearch_item_edit_itemClicked(QListWidgetItem *item)
{
    QString id = item->data(Qt::UserRole).toString();
    auto itemOpt = m_db.getItemById(id.toStdString());
    if (itemOpt.has_value()) {
        ui->txtName_item_edit->setText(QString::fromStdString(itemOpt->name));
        ui->txtQuantity_item_edit->setText(QString::number(itemOpt->quantity));
        ui->txtPrice_item_edit->setText(QString::number(itemOpt->price, 'f', 2));
        ui->txtCategory_item_edit->setText(QString::fromStdString(itemOpt->category));
        ui->txtShelf_item_edit->setText(QString::fromStdString(itemOpt->shelf));
        ui->txtId_item_edit->setText(QString::fromStdString(itemOpt->id));
        if (ui->cboStatus_item_edit) {
            int idx = ui->cboStatus_item_edit->findText(QString::fromStdString(itemOpt->status));
            if (idx >= 0) ui->cboStatus_item_edit->setCurrentIndex(idx);
        }
    }
}

void MainWindow::on_btnEdit_item_clicked()
{
    if (!checkRoleRequired(BusinessLogic::RequiredRole::Admin)) return;

    // Preserve original createdAt from the loaded item
    QString id = ui->txtId_item_edit->text().trimmed();
    auto existingItem = m_db.getItemById(id.toStdString());
    std::string origCreatedAt = existingItem.has_value()
        ? Domain::toISOString(existingItem->createdAt)
        : Domain::toISOString(Domain::now());

    DTO::ItemDTO itemDto;
    itemDto.id = id.toStdString();
    itemDto.name = ui->txtName_item_edit->text().trimmed().toStdString();
    itemDto.quantity = ui->txtQuantity_item_edit->text().trimmed().toInt();
    itemDto.price = ui->txtPrice_item_edit->text().trimmed().toDouble();
    itemDto.category = ui->txtCategory_item_edit->text().trimmed().toStdString();
    itemDto.shelf = ui->txtShelf_item_edit->text().trimmed().toStdString();
    itemDto.status = ui->cboStatus_item_edit ? ui->cboStatus_item_edit->currentText().toStdString() : "In Stock";
    itemDto.createdAt = origCreatedAt;
    itemDto.updatedAt = Domain::toISOString(Domain::now());

    auto result = BusinessLogic::updateItem(m_db, itemDto);
    if (result.isValid) {
        QMessageBox::information(this, "Edit Item", "Item updated successfully.");
        LOG_INFO("Item updated: " + QString::fromStdString(itemDto.name));
        m_worklog.logEntry(WorklogEntry::ActionType::Edit, WorklogEntry::EntityType::Item,
                          itemDto.id, "Edited item: " + itemDto.name);
    } else {
        QMessageBox::warning(this, "Edit Item", QString::fromStdString(result.errorMessage));
    }
}

void MainWindow::on_btnClear_name_item_edit_clicked() { ui->txtName_item_edit->clear(); }
void MainWindow::on_btnClear_id_item_edit_clicked() { ui->txtId_item_edit->clear(); }

// ═══════════════════════════════════════════════════════════════════
// Items — Remove
// ═══════════════════════════════════════════════════════════════════

void MainWindow::on_actionRemove_Items_triggered()
{
    if (!checkLoginRequired()) return;
    if (ui->stackedWidget) ui->stackedWidget->setCurrentIndex(4); // Remove Item page
}

void MainWindow::on_btnSearch_item_remove_clicked()
{
    QString term = ui->txtSearch_item_remove->text().trimmed();
    auto items = BusinessLogic::populateList(m_db, "items", term.toStdString(), "");
    ui->lstSearch_item_remove->clear();
    for (const auto& item : items) {
        QListWidgetItem* lwi = new QListWidgetItem(QString::fromStdString(item.displayText));
        lwi->setData(Qt::UserRole, QString::fromStdString(item.id));
        ui->lstSearch_item_remove->addItem(lwi);
    }
}

void MainWindow::on_lstSearch_item_remove_itemClicked(QListWidgetItem *item)
{
    QString id = item->data(Qt::UserRole).toString();
    auto itemOpt = m_db.getItemById(id.toStdString());
    if (itemOpt.has_value()) {
        ui->txtName_item_remove->setText(QString::fromStdString(itemOpt->name));
        ui->txtId_item_remove->setText(QString::fromStdString(itemOpt->id));
    }
}

void MainWindow::on_btnRemove_item_clicked()
{
    if (!checkRoleRequired(BusinessLogic::RequiredRole::Admin)) return;

    QString id = ui->txtId_item_remove->text().trimmed();
    QString name = ui->txtName_item_remove->text().trimmed();

    if (QMessageBox::question(this, "Confirm Remove",
        "Remove item \"" + name + "\"?") == QMessageBox::Yes) {
        if (m_db.removeItem(id.toStdString())) {
            QMessageBox::information(this, "Remove Item", "Item removed successfully.");
            LOG_INFO("Item removed: " + name);
            m_worklog.logEntry(WorklogEntry::ActionType::Remove, WorklogEntry::EntityType::Item,
                              id.toStdString(), "Removed item: " + name.toStdString());
        } else {
            QMessageBox::warning(this, "Remove Item", "Failed to remove item.");
        }
    }
}

void MainWindow::on_btnUndoRemove_item_clicked()
{
    if (!checkRoleRequired(BusinessLogic::RequiredRole::Admin)) return;
    if (ui->stackedWidget) ui->stackedWidget->setCurrentIndex(5); // Undo removed page
}

void MainWindow::on_btnUndoLast_item_clicked()
{
    if (!checkRoleRequired(BusinessLogic::RequiredRole::Admin)) return;
    auto removed = m_db.getRemovedItems();
    if (!removed.empty()) {
        QString id = QString::fromStdString(removed.front().id);
        if (m_db.restoreItem(id.toStdString())) {
            QMessageBox::information(this, "Undo Remove", "Item restored.");
        }
    }
}

// ═══════════════════════════════════════════════════════════════════
// Undo — Removed Items
// ═══════════════════════════════════════════════════════════════════

void MainWindow::on_actionUndo_Removed_Items_triggered()
{
    if (!checkRoleRequired(BusinessLogic::RequiredRole::Admin)) return;
    if (ui->stackedWidget) ui->stackedWidget->setCurrentIndex(5);

    auto removed = m_db.getRemovedItems();
    ui->lstSearch_undoremoved->clear();
    for (const auto& item : removed) {
        QListWidgetItem* lwi = new QListWidgetItem(QString::fromStdString(item.toDisplayString()));
        lwi->setData(Qt::UserRole, QString::fromStdString(item.id));
        ui->lstSearch_undoremoved->addItem(lwi);
    }
}

void MainWindow::on_lstSearch_undoremoved_itemClicked(QListWidgetItem *item)
{
    QString id = item->data(Qt::UserRole).toString();
    auto itemOpt = m_db.getItemById(id.toStdString());
    if (!itemOpt.has_value()) {
        // Might be in removed_items
    }
}

void MainWindow::on_btnUndoSelected_undoremoved_clicked()
{
    if (!checkRoleRequired(BusinessLogic::RequiredRole::Admin)) return;
    auto selected = ui->lstSearch_undoremoved->currentItem();
    if (selected) {
        QString id = selected->data(Qt::UserRole).toString();
        if (m_db.restoreItem(id.toStdString())) {
            QMessageBox::information(this, "Undo", "Item restored.");
            on_actionUndo_Removed_Items_triggered(); // refresh
        }
    }
}

void MainWindow::on_btnUndoAll_undoremoved_clicked()
{
    if (!checkRoleRequired(BusinessLogic::RequiredRole::Admin)) return;
    auto removed = m_db.getRemovedItems();
    int count = 0;
    for (const auto& item : removed) {
        if (m_db.restoreItem(item.id)) count++;
    }
    QMessageBox::information(this, "Undo All", QString("%1 item(s) restored.").arg(count));
    on_actionUndo_Removed_Items_triggered();
}

// ═══════════════════════════════════════════════════════════════════
// Sell Item (POS)
// ═══════════════════════════════════════════════════════════════════

void MainWindow::on_actionSell_Item_triggered()
{
    if (!checkLoginRequired()) return;
    if (ui->stackedWidget) ui->stackedWidget->setCurrentIndex(6); // POS page

    // Populate item grid
    auto items = m_db.getAllItems();
    rebuildItemGrid(items);
}

void MainWindow::on_btnSearch_sell_clicked()
{
    QString term = ui->txtSearch_sell->text().trimmed();
    std::vector<Domain::Item> items;
    if (term.isEmpty()) {
        items = m_db.getAllItems();
    } else {
        items = m_db.searchItems(term.toStdString(), "");
    }
    rebuildItemGrid(items);
}

void MainWindow::rebuildItemGrid(const std::vector<Domain::Item>& items)
{
    if (!ui->gridLayoutItemScroll) return;

    // Clear existing grid
    QLayoutItem* child;
    while ((child = ui->gridLayoutItemScroll->takeAt(0)) != nullptr) {
        if (child->widget()) child->widget()->deleteLater();
        delete child;
    }

    int width = ui->scrollAreaItems->viewport()->width();
    int cols = qMax(1, width / 260);

    int row = 0, col = 0;
    for (const auto& item : items) {
        QWidget* card = createItemCard(item);
        ui->gridLayoutItemScroll->addWidget(card, row, col);
        col++;
        if (col >= cols) { col = 0; row++; }
    }
}

QWidget* MainWindow::createItemCard(const Domain::Item& item)
{
    QFrame* card = new QFrame();
    card->setFrameShape(QFrame::StyledPanel);
    card->setMinimumSize(240, 200);
    card->setStyleSheet(
        "QFrame { background: white; border: 2px solid #ddd; border-radius: 10px; margin: 5px; }"
        "QFrame:hover { border-color: #0078d4; }"
    );

    QVBoxLayout* layout = new QVBoxLayout(card);

    // Item name
    QLabel* nameLbl = new QLabel(QString::fromStdString(item.name));
    QFont nameFont = nameLbl->font();
    nameFont.setPointSize(14);
    nameFont.setBold(true);
    nameLbl->setFont(nameFont);
    nameLbl->setWordWrap(true);
    layout->addWidget(nameLbl);

    // Price
    QLabel* priceLbl = new QLabel("$" + QString::number(item.price, 'f', 2));
    QFont priceFont = priceLbl->font();
    priceFont.setPointSize(18);
    priceFont.setBold(true);
    priceLbl->setFont(priceFont);
    priceLbl->setStyleSheet("color: #2e7d32;");
    layout->addWidget(priceLbl);

    // Quantity & status
    QString statusColor = "#2e7d32";
    if (item.status == "Low Stock") statusColor = "#f57f17";
    else if (item.status == "Out of Stock" || item.status == "Sold Out") statusColor = "#c62828";

    QLabel* qtyLbl = new QLabel("Qty: " + QString::number(item.quantity) +
                                 "  |  " + QString::fromStdString(item.status));
    qtyLbl->setStyleSheet("color: " + statusColor + "; font-size: 12px;");
    layout->addWidget(qtyLbl);

    // Shelf/Category
    if (!item.shelf.empty() || !item.category.empty()) {
        QString info = QString::fromStdString(item.shelf);
        if (!item.category.empty()) info += "  |  " + QString::fromStdString(item.category);
        QLabel* infoLbl = new QLabel(info);
        infoLbl->setStyleSheet("color: #666; font-size: 11px;");
        layout->addWidget(infoLbl);
    }

    layout->addStretch();

    // Sell button — large touch target
    QPushButton* sellBtn = new QPushButton("SELL");
    sellBtn->setMinimumHeight(50);
    sellBtn->setCursor(Qt::PointingHandCursor);
    sellBtn->setStyleSheet(
        "QPushButton { background-color: #0078d4; color: white; font-size: 16px; "
        "font-weight: bold; border: none; border-radius: 8px; padding: 8px; }"
        "QPushButton:hover { background-color: #005a9e; }"
        "QPushButton:pressed { background-color: #003f7f; }"
        "QPushButton:disabled { background-color: #ccc; color: #666; }"
    );
    sellBtn->setEnabled(item.quantity > 0 && item.status != "Sold Out");

    // Connect sell button
    std::string itemId = item.id;
    QPointer<MainWindow> safeSelf = this;
    connect(sellBtn, &QPushButton::clicked, [safeSelf, itemId]() {
        if (!safeSelf) return;  // window destroyed
        safeSelf->m_selectedSellItemId = QString::fromStdString(itemId);
        // Show quantity dialog
        bool ok;
        int qty = QInputDialog::getInt(safeSelf, "Sell Item", "Quantity to sell:", 1, 1, 999, 1, &ok);
        if (ok && qty > 0) {
            auto result = BusinessLogic::sellItem(
                safeSelf->m_db, itemId, qty,
                safeSelf->m_currentUser.value_or(Domain::User{}).id);
            if (result.isValid) {
                QMessageBox::information(safeSelf, "Sale", "Item sold successfully!");
                safeSelf->m_worklog.logEntry(WorklogEntry::ActionType::Sale, WorklogEntry::EntityType::Sale,
                                        itemId, "Sold item");
                // Refresh grid
                auto items = safeSelf->m_db.getAllItems();
                safeSelf->rebuildItemGrid(items);
            } else {
                QMessageBox::warning(safeSelf, "Sale Error", QString::fromStdString(result.errorMessage));
            }
        }
    });

    layout->addWidget(sellBtn);

    return card;
}

void MainWindow::on_btnSellItem_clicked()
{
    // Fallback: manual sell by ID
    if (!checkLoginRequired()) return;
    // This is handled by the grid card sell buttons
}

void MainWindow::on_lstSearch_sell_itemClicked(QListWidgetItem *item)
{
    Q_UNUSED(item);
}

// ═══════════════════════════════════════════════════════════════════
// Categories
// ═══════════════════════════════════════════════════════════════════

void MainWindow::on_actionManage_Categories_triggered()
{
    if (!checkLoginRequired()) return;
    if (ui->stackedWidget) ui->stackedWidget->setCurrentIndex(7); // Categories page
    auto cats = m_db.getAllCategories();
    ui->lstSearch_category->clear();
    for (const auto& c : cats) {
        QListWidgetItem* lwi = new QListWidgetItem(QString::fromStdString(c.name));
        lwi->setData(Qt::UserRole, QString::fromStdString(c.id));
        ui->lstSearch_category->addItem(lwi);
    }
}

void MainWindow::on_lstSearch_category_itemClicked(QListWidgetItem *item)
{
    ui->txtName_category->setText(item->text());
    ui->txtId_category->setText(item->data(Qt::UserRole).toString());
}

void MainWindow::on_btnAdd_category_clicked()
{
    if (!checkRoleRequired(BusinessLogic::RequiredRole::Admin)) return;
    QString name = ui->txtName_category->text().trimmed();
    QString id = QString::number(std::chrono::duration_cast<std::chrono::milliseconds>(
        std::chrono::system_clock::now().time_since_epoch()).count());

    DTO::CategoryDTO dto;
    dto.id = id.toStdString();
    dto.name = name.toStdString();
    auto result = BusinessLogic::addCategory(m_db, dto);
    if (result.isValid) {
        QMessageBox::information(this, "Category", "Category added.");
        m_worklog.logEntry(WorklogEntry::ActionType::Add, WorklogEntry::EntityType::Category,
                          id.toStdString(), "Added category: " + name.toStdString());
        on_actionManage_Categories_triggered();
    } else {
        QMessageBox::warning(this, "Category", QString::fromStdString(result.errorMessage));
    }
}

void MainWindow::on_btnEdit_category_clicked()
{
    if (!checkRoleRequired(BusinessLogic::RequiredRole::Admin)) return;
    DTO::CategoryDTO dto;
    dto.id = ui->txtId_category->text().trimmed().toStdString();
    dto.name = ui->txtName_category->text().trimmed().toStdString();
    auto result = BusinessLogic::updateCategory(m_db, dto);
    if (result.isValid) {
        QMessageBox::information(this, "Category", "Category updated.");
        m_worklog.logEntry(WorklogEntry::ActionType::Edit, WorklogEntry::EntityType::Category,
                          dto.id, "Edited category: " + dto.name);
        on_actionManage_Categories_triggered();
    } else {
        QMessageBox::warning(this, "Category", QString::fromStdString(result.errorMessage));
    }
}

void MainWindow::on_btnRemove_category_clicked()
{
    if (!checkRoleRequired(BusinessLogic::RequiredRole::Admin)) return;
    QString id = ui->txtId_category->text().trimmed();
    if (QMessageBox::question(this, "Confirm", "Remove this category?") == QMessageBox::Yes) {
        if (m_db.removeCategory(id.toStdString())) {
            QMessageBox::information(this, "Category", "Category removed.");
            m_worklog.logEntry(WorklogEntry::ActionType::Remove, WorklogEntry::EntityType::Category,
                              id.toStdString(), "Removed category");
            on_actionManage_Categories_triggered();
        }
    }
}

void MainWindow::on_btnClear_name_category_clicked() { ui->txtName_category->clear(); }
void MainWindow::on_btnUndoAdd_category_clicked() { on_actionManage_Categories_triggered(); }
void MainWindow::on_btnUndoEdit_category_clicked() { on_actionManage_Categories_triggered(); }
void MainWindow::on_btnUndoRemove_category_clicked() { on_actionManage_Categories_triggered(); }

// ═══════════════════════════════════════════════════════════════════
// Shelves
// ═══════════════════════════════════════════════════════════════════

void MainWindow::on_actionManage_Shelves_triggered()
{
    if (!checkLoginRequired()) return;
    if (ui->stackedWidget) ui->stackedWidget->setCurrentIndex(8); // Shelves page
    auto shelves = m_db.getAllShelves();
    ui->lstSearch_shelf->clear();
    for (const auto& s : shelves) {
        QListWidgetItem* lwi = new QListWidgetItem(QString::fromStdString(s.name));
        lwi->setData(Qt::UserRole, QString::fromStdString(s.id));
        ui->lstSearch_shelf->addItem(lwi);
    }
}

void MainWindow::on_lstSearch_shelf_itemClicked(QListWidgetItem *item)
{
    ui->txtName_shelf->setText(item->text());
    ui->txtId_shelf->setText(item->data(Qt::UserRole).toString());
}

void MainWindow::on_btnAdd_shelf_clicked()
{
    if (!checkRoleRequired(BusinessLogic::RequiredRole::Admin)) return;
    QString name = ui->txtName_shelf->text().trimmed();
    QString id = QString::number(std::chrono::duration_cast<std::chrono::milliseconds>(
        std::chrono::system_clock::now().time_since_epoch()).count());

    DTO::ShelfDTO dto;
    dto.id = id.toStdString();
    dto.name = name.toStdString();
    auto result = BusinessLogic::addShelf(m_db, dto);
    if (result.isValid) {
        QMessageBox::information(this, "Shelf", "Shelf added.");
        m_worklog.logEntry(WorklogEntry::ActionType::Add, WorklogEntry::EntityType::Shelf,
                          id.toStdString(), "Added shelf: " + name.toStdString());
        on_actionManage_Shelves_triggered();
    } else {
        QMessageBox::warning(this, "Shelf", QString::fromStdString(result.errorMessage));
    }
}

void MainWindow::on_btnEdit_shelf_clicked()
{
    if (!checkRoleRequired(BusinessLogic::RequiredRole::Admin)) return;
    DTO::ShelfDTO dto;
    dto.id = ui->txtId_shelf->text().trimmed().toStdString();
    dto.name = ui->txtName_shelf->text().trimmed().toStdString();
    auto result = BusinessLogic::updateShelf(m_db, dto);
    if (result.isValid) {
        QMessageBox::information(this, "Shelf", "Shelf updated.");
        m_worklog.logEntry(WorklogEntry::ActionType::Edit, WorklogEntry::EntityType::Shelf,
                          dto.id, "Edited shelf: " + dto.name);
        on_actionManage_Shelves_triggered();
    } else {
        QMessageBox::warning(this, "Shelf", QString::fromStdString(result.errorMessage));
    }
}

void MainWindow::on_btnRemove_shelf_clicked()
{
    if (!checkRoleRequired(BusinessLogic::RequiredRole::Admin)) return;
    QString id = ui->txtId_shelf->text().trimmed();
    if (QMessageBox::question(this, "Confirm", "Remove this shelf?") == QMessageBox::Yes) {
        if (m_db.removeShelf(id.toStdString())) {
            QMessageBox::information(this, "Shelf", "Shelf removed.");
            m_worklog.logEntry(WorklogEntry::ActionType::Remove, WorklogEntry::EntityType::Shelf,
                              id.toStdString(), "Removed shelf");
            on_actionManage_Shelves_triggered();
        }
    }
}

void MainWindow::on_btnClear_name_shelf_clicked() { ui->txtName_shelf->clear(); }
void MainWindow::on_btnUndoAdd_shelf_clicked() { on_actionManage_Shelves_triggered(); }
void MainWindow::on_btnUndoEdit_shelf_clicked() { on_actionManage_Shelves_triggered(); }
void MainWindow::on_btnUndoRemove_shelf_clicked() { on_actionManage_Shelves_triggered(); }

// ═══════════════════════════════════════════════════════════════════
// Sales History
// ═══════════════════════════════════════════════════════════════════

void MainWindow::on_actionSales_History_triggered()
{
    if (!checkLoginRequired()) return;
    if (ui->stackedWidget) ui->stackedWidget->setCurrentIndex(9); // Sales History page
    on_btnRefresh_sales_clicked();
}

void MainWindow::on_btnRefresh_sales_clicked()
{
    auto sales = m_db.getAllSales();
    ui->lstSearch_sales->clear();
    double totalRevenue = 0.0;
    for (const auto& s : sales) {
        QListWidgetItem* lwi = new QListWidgetItem(QString::fromStdString(s.toDisplayString()));
        lwi->setData(Qt::UserRole, QString::fromStdString(s.id));
        ui->lstSearch_sales->addItem(lwi);
        totalRevenue += s.totalAmount;
    }
    if (ui->lblSalesTotal) {
        ui->lblSalesTotal->setText("Total Revenue: $" + QString::number(totalRevenue, 'f', 2));
    }
}

void MainWindow::on_btnSearch_sales_clicked()
{
    QString term = ui->txtSearch_sales->text().trimmed();
    auto sales = m_db.searchSales(term.toStdString(), "");
    ui->lstSearch_sales->clear();
    double totalRevenue = 0.0;
    for (const auto& s : sales) {
        QListWidgetItem* lwi = new QListWidgetItem(QString::fromStdString(s.toDisplayString()));
        lwi->setData(Qt::UserRole, QString::fromStdString(s.id));
        ui->lstSearch_sales->addItem(lwi);
        totalRevenue += s.totalAmount;
    }
    if (ui->lblSalesTotal) {
        ui->lblSalesTotal->setText("Search Total: $" + QString::number(totalRevenue, 'f', 2));
    }
}

void MainWindow::on_btnExport_sales_clicked()
{
    QString fileName = QFileDialog::getSaveFileName(this, "Export Sales", "sales_export.csv", "CSV Files (*.csv)");
    if (fileName.isEmpty()) return;

    QFile file(fileName);
    if (!file.open(QIODevice::WriteOnly | QIODevice::Text)) return;

    QTextStream out(&file);
    out << "Sale ID,Item ID,Quantity Sold,Unit Price,Total Amount,Sold By,Sale Date\n";

    auto sales = m_db.getAllSales();
    for (const auto& s : sales) {
        out << QString::fromStdString(s.id) << ","
            << QString::fromStdString(s.itemId) << ","
            << s.quantitySold << ","
            << QString::number(s.unitPrice, 'f', 2) << ","
            << QString::number(s.totalAmount, 'f', 2) << ","
            << QString::fromStdString(s.soldBy) << ","
            << QString::fromStdString(Domain::toISOString(s.saleDate)) << "\n";
    }
    file.close();
    QMessageBox::information(this, "Export", "Sales exported to " + fileName);
}

// ═══════════════════════════════════════════════════════════════════
// Reports
// ═══════════════════════════════════════════════════════════════════

void MainWindow::on_actionMake_Report_triggered()
{
    if (!checkLoginRequired()) return;
    if (ui->stackedWidget) ui->stackedWidget->setCurrentIndex(10); // Report page
}

void MainWindow::on_btnGenerateReport_clicked()
{
    if (!checkRoleRequired(BusinessLogic::RequiredRole::Admin)) return;

    auto sales = m_db.getAllSales();
    auto items = m_db.getAllItems();

    double totalRevenue = 0.0;
    int totalItemsSold = 0;
    for (const auto& s : sales) {
        totalRevenue += s.totalAmount;
        totalItemsSold += s.quantitySold;
    }

    QString report;
    report += "═══════════════════════════════════════\n";
    report += "         QMARK — SALES REPORT\n";
    report += "═══════════════════════════════════════\n";
    report += "Generated: " + QDateTime::currentDateTime().toString("yyyy-MM-dd hh:mm:ss") + "\n\n";
    report += "Total Revenue: $" + QString::number(totalRevenue, 'f', 2) + "\n";
    report += "Total Transactions: " + QString::number(sales.size()) + "\n";
    report += "Total Items Sold: " + QString::number(totalItemsSold) + "\n\n";

    report += "Items in stock: " + QString::number(items.size()) + "\n";
    int lowStock = 0, outOfStock = 0;
    for (const auto& item : items) {
        if (item.status == "Low Stock") lowStock++;
        if (item.status == "Out of Stock" || item.status == "Sold Out") outOfStock++;
    }
    report += "Low Stock: " + QString::number(lowStock) + "\n";
    report += "Out of Stock: " + QString::number(outOfStock) + "\n";

    if (ui->txtReport) {
        ui->txtReport->setText(report);
    }
}

void MainWindow::on_btnExportReport_clicked()
{
    if (ui->txtReport) {
        QString fileName = QFileDialog::getSaveFileName(this, "Export Report", "report.txt", "Text Files (*.txt)");
        if (!fileName.isEmpty()) {
            QFile file(fileName);
            if (file.open(QIODevice::WriteOnly | QIODevice::Text)) {
                QTextStream out(&file);
                out << ui->txtReport->toPlainText();
                file.close();
                QMessageBox::information(this, "Export", "Report saved.");
            }
        }
    }
}

// ═══════════════════════════════════════════════════════════════════
// Database Selection
// ═══════════════════════════════════════════════════════════════════

void MainWindow::on_actionDatabase_Selection_triggered()
{
    if (!checkRoleRequired(BusinessLogic::RequiredRole::SuperAdmin)) return;
    if (ui->stackedWidget) ui->stackedWidget->setCurrentIndex(11); // DB Selection page
}

void MainWindow::on_actionCreate_Database_triggered()
{
    on_actionDatabase_Selection_triggered();
}

void MainWindow::on_btnBrowseItemsDb_clicked()
{
    QString file = QFileDialog::getSaveFileName(this, "Select Items Database", "items.db", "SQLite DB (*.db)");
    if (!file.isEmpty() && ui->txtItemsDbPath) {
        ui->txtItemsDbPath->setText(file);
    }
}

void MainWindow::on_btnBrowseUsersDb_clicked()
{
    QString file = QFileDialog::getSaveFileName(this, "Select Users Database", "users.db", "SQLite DB (*.db)");
    if (!file.isEmpty() && ui->txtUsersDbPath) {
        ui->txtUsersDbPath->setText(file);
    }
}

void MainWindow::on_btnLoadDbConfig_clicked()
{
    QString itemsDb = ui->txtItemsDbPath ? ui->txtItemsDbPath->text() : "";
    QString usersDb = ui->txtUsersDbPath ? ui->txtUsersDbPath->text() : "";

    if (itemsDb.isEmpty() || usersDb.isEmpty()) {
        QMessageBox::warning(this, "Database", "Please select both database files.");
        return;
    }

    BusinessLogic::shutdownDatabases(m_db);
    if (BusinessLogic::initializeDatabases(m_db, itemsDb.toStdString(), usersDb.toStdString())) {
        QMessageBox::information(this, "Database", "Connected to databases successfully.");
        LOG_INFO("Databases loaded: " + itemsDb + ", " + usersDb);
    } else {
        QMessageBox::warning(this, "Database", "Failed to connect to databases.");
    }
}

void MainWindow::on_btnSaveAsDefault_clicked()
{
    QSettings settings("QMark", "SchoolShop");
    settings.setValue("db/itemsPath", ui->txtItemsDbPath ? ui->txtItemsDbPath->text() : "");
    settings.setValue("db/usersPath", ui->txtUsersDbPath ? ui->txtUsersDbPath->text() : "");
    QMessageBox::information(this, "Settings", "Default database configuration saved.");
}

void MainWindow::on_btnTestConnection_clicked()
{
    if (m_db.isConnected()) {
        QMessageBox::information(this, "Test", "Database connection is active.");
    } else {
        QMessageBox::warning(this, "Test", "No database connection.");
    }
}

void MainWindow::on_btnCreateNewDb_clicked()
{
    QString itemsDb = ui->txtItemsDbPath ? ui->txtItemsDbPath->text() : "items.db";
    QString usersDb = ui->txtUsersDbPath ? ui->txtUsersDbPath->text() : "users.db";

    BusinessLogic::shutdownDatabases(m_db);
    if (BusinessLogic::initializeDatabases(m_db, itemsDb.toStdString(), usersDb.toStdString())) {
        QMessageBox::information(this, "Create DB", "New databases created and connected.");
    } else {
        QMessageBox::warning(this, "Create DB", "Failed to create databases.");
    }
}

void MainWindow::on_chkTelemetry_toggled(bool checked)
{
    QSettings settings("QMark", "SchoolShop");
    settings.setValue("telemetry/enabled", checked);
    AppLogger::instance().setTelemetryEnabled(checked);
    if (checked) {
        QString logDir = QDir::currentPath();
        QString ts = QDateTime::currentDateTime().toString("yyyyMMdd_hhmmss");
        QString logPath = logDir + "/telemetry_" + ts + ".log";
        QString csvPath = logDir + "/telemetry_" + ts + ".csv";
        QString dbPath  = logDir + "/telemetry_" + ts + ".db";
        AppLogger::instance().setLogFile(logPath);
        telemetry().open(csvPath, dbPath);
        telemetry().logInfo("Telemetry enabled by user");
    } else {
        telemetry().logInfo("Telemetry disabled by user");
        telemetry().close();
    }
}

// ═══════════════════════════════════════════════════════════════════
// Accounts
// ═══════════════════════════════════════════════════════════════════

void MainWindow::on_actionAccounts_triggered()
{
    if (!checkRoleRequired(BusinessLogic::RequiredRole::SuperAdmin)) return;
    if (ui->stackedWidget) ui->stackedWidget->setCurrentIndex(12); // Accounts page

    auto users = m_db.getAllUsers();
    ui->lstSearchAccount->clear();
    for (const auto& u : users) {
        QListWidgetItem* lwi = new QListWidgetItem(
            QString::fromStdString(u.username) + " | " + QString::fromStdString(u.roleName()));
        lwi->setData(Qt::UserRole, QString::fromStdString(u.id));
        ui->lstSearchAccount->addItem(lwi);
    }
}

void MainWindow::on_btnSearchAccount_clicked()
{
    on_actionAccounts_triggered();
}

void MainWindow::on_btnChangeRole_clicked()
{
    if (!checkRoleRequired(BusinessLogic::RequiredRole::SuperAdmin)) return;
    auto selected = ui->lstSearchAccount->currentItem();
    if (!selected) return;

    QString userId = selected->data(Qt::UserRole).toString();
    // Simple role change via combo or dialog
    QStringList roles = {"Clerk", "Admin", "SuperAdmin"};
    bool ok;
    QString newRole = QInputDialog::getItem(this, "Change Role", "Select new role:", roles, 0, false, &ok);
    if (ok && !newRole.isEmpty()) {
        // Find user and update
        auto users = m_db.getAllUsers();
        for (auto& u : users) {
            if (u.id == userId.toStdString()) {
                if (newRole == "SuperAdmin") u.role = Domain::User::Role::SuperAdmin;
                else if (newRole == "Admin") u.role = Domain::User::Role::Admin;
                else u.role = Domain::User::Role::UserRole;
                m_db.updateUser(u);
                QMessageBox::information(this, "Account", "Role changed.");
                on_actionAccounts_triggered();
                break;
            }
        }
    }
}

void MainWindow::on_btnChangePassword_clicked()
{
    if (!checkRoleRequired(BusinessLogic::RequiredRole::SuperAdmin)) return;
    auto selected = ui->lstSearchAccount->currentItem();
    if (!selected) return;

    bool ok;
    QString newPwd = QInputDialog::getText(this, "Change Password", "New password:", QLineEdit::Password, "", &ok);
    if (ok && !newPwd.isEmpty()) {
        QString userId = selected->data(Qt::UserRole).toString();
        auto users = m_db.getAllUsers();
        for (auto& u : users) {
            if (u.id == userId.toStdString()) {
                u.passwordHash = newPwd.toStdString();
                m_db.updateUser(u);
                QMessageBox::information(this, "Account", "Password changed.");
                break;
            }
        }
    }
}

void MainWindow::on_btnDeleteAccount_clicked()
{
    if (!checkRoleRequired(BusinessLogic::RequiredRole::SuperAdmin)) return;
    // Users cannot be deleted in current implementation — just a placeholder
    QMessageBox::information(this, "Account", "Account deletion not yet implemented.");
}

// ═══════════════════════════════════════════════════════════════════
// Preferences
// ═══════════════════════════════════════════════════════════════════

void MainWindow::on_actionPreferences_triggered()
{
    if (ui->stackedWidget) ui->stackedWidget->setCurrentIndex(13); // Preferences page
}

void MainWindow::on_btnSavePreferences_clicked()
{
    QSettings settings("QMark", "SchoolShop");
    settings.setValue("worklog/enabled", ui->chkWorklog->isChecked());
    settings.setValue("telemetry/enabled", ui->chkTelemetry->isChecked());
    QMessageBox::information(this, "Preferences", "Preferences saved.");
}

void MainWindow::on_btnResetPreferences_clicked()
{
    QSettings settings("QMark", "SchoolShop");
    settings.clear();
    ui->chkWorklog->setChecked(false);
    ui->chkTelemetry->setChecked(false);
    QMessageBox::information(this, "Preferences", "Preferences reset to defaults.");
}

void MainWindow::on_chkWorklog_toggled(bool checked)
{
    m_worklog.setEnabled(checked);
    if (checked) {
        QString logPath = QDir::currentPath() + "/" + Worklog::generateSessionFileName();
        m_worklog.setLogFile(logPath);
    }
}

// ═══════════════════════════════════════════════════════════════════
// Worklog Stats
// ═══════════════════════════════════════════════════════════════════

void MainWindow::on_actionWorklogStats_triggered()
{
    if (!checkRoleRequired(BusinessLogic::RequiredRole::Admin)) return;
    if (ui->stackedWidget) ui->stackedWidget->setCurrentIndex(14); // Worklog page
    on_btnRefreshWorklog_clicked();
}

void MainWindow::on_btnRefreshWorklog_clicked()
{
    if (ui->txtWorklog) {
        QString stats;
        stats += "═══════════════════════════════════════\n";
        stats += "       WORKLOG SESSION STATISTICS\n";
        stats += "═══════════════════════════════════════\n";
        stats += "Session Start: " + m_worklog.getSessionStart().toString("yyyy-MM-dd hh:mm:ss") + "\n\n";
        stats += "Items Added:      " + QString::number(m_worklog.getItemAddCount()) + "\n";
        stats += "Items Edited:     " + QString::number(m_worklog.getItemEditCount()) + "\n";
        stats += "Items Removed:    " + QString::number(m_worklog.getItemRemoveCount()) + "\n";
        stats += "Sales Recorded:   " + QString::number(m_worklog.getSaleCount()) + "\n";
        stats += "Categories Added: " + QString::number(m_worklog.getCategoryAddCount()) + "\n";
        stats += "Categories Edited: " + QString::number(m_worklog.getCategoryEditCount()) + "\n";
        stats += "Categories Removed:" + QString::number(m_worklog.getCategoryRemoveCount()) + "\n";
        stats += "Shelves Added:    " + QString::number(m_worklog.getShelfAddCount()) + "\n";
        stats += "Shelves Edited:   " + QString::number(m_worklog.getShelfEditCount()) + "\n";
        stats += "Shelves Removed:  " + QString::number(m_worklog.getShelfRemoveCount()) + "\n";
        stats += "Users Added:      " + QString::number(m_worklog.getUserAddCount()) + "\n";
        stats += "Users Edited:     " + QString::number(m_worklog.getUserEditCount()) + "\n";
        stats += "Users Removed:    " + QString::number(m_worklog.getUserRemoveCount()) + "\n";
        ui->txtWorklog->setText(stats);
    }
}

void MainWindow::on_btnExportWorklog_clicked()
{
    QString fileName = QFileDialog::getSaveFileName(this, "Export Worklog", "worklog_export.txt", "Text Files (*.txt)");
    if (fileName.isEmpty()) return;

    QFile file(fileName);
    if (!file.open(QIODevice::WriteOnly | QIODevice::Text)) return;

    QTextStream out(&file);
    out << ui->txtWorklog->toPlainText();
    file.close();
    QMessageBox::information(this, "Export", "Worklog exported.");
}

// ═══════════════════════════════════════════════════════════════════
// Troubleshoot
// ═══════════════════════════════════════════════════════════════════

void MainWindow::on_actionTroubleshoot_triggered()
{
    if (!checkRoleRequired(BusinessLogic::RequiredRole::Admin)) return;
    if (ui->stackedWidget) ui->stackedWidget->setCurrentIndex(15); // Troubleshoot page
}

void MainWindow::on_btnTestDbConnection_clicked()
{
    if (m_db.isConnected()) {
        QMessageBox::information(this, "DB Test", "Connected.");
    } else {
        QMessageBox::warning(this, "DB Test", "Not connected.");
    }
}

void MainWindow::on_btnViewLogs_clicked()
{
    QString logDir = QDir::currentPath();
    QMessageBox::information(this, "Logs", "Log files are located in:\n" + logDir);
}

void MainWindow::on_btnExportDiagnostics_clicked()
{
    QString fileName = QFileDialog::getSaveFileName(this, "Export Diagnostics", "diagnostics.txt", "Text Files (*.txt)");
    if (fileName.isEmpty()) return;

    QFile file(fileName);
    if (!file.open(QIODevice::WriteOnly | QIODevice::Text)) return;

    QTextStream out(&file);
    out << "=== QMark Diagnostics ===\n";
    out << "Date: " << QDateTime::currentDateTime().toString("yyyy-MM-dd hh:mm:ss") << "\n";
    out << "DB Connected: " << (m_db.isConnected() ? "Yes" : "No") << "\n";
    out << "Logged In: " << (m_isLoggedIn ? "Yes" : "No") << "\n";
    if (m_currentUser.has_value()) {
        out << "User: " << QString::fromStdString(m_currentUser->username) << "\n";
        out << "Role: " << QString::fromStdString(m_currentUser->roleName()) << "\n";
    }
    out << "Telemetry: " << (AppLogger::instance().isTelemetryEnabled() ? "On" : "Off") << "\n";
    out << "Worklog: " << (m_worklog.isEnabled() ? "On" : "Off") << "\n";
    file.close();
    QMessageBox::information(this, "Diagnostics", "Diagnostics exported.");
}

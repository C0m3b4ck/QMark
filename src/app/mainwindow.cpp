#include "mainwindow.h"
#include "ui_mainwindow.h"
#include "sqlite_dataaccess.h"
#include "businesslogic.h"
#include "crypto.h"
#include "logger.h"
#include "telemetry.h"
#include "translations.h"
#include "charts.h"
#include "statistics.h"
#include "pdf_export.h"

// Automation & remote-feature modules.
#include "appsettings.h"
#include "backup.h"
#include "netutil.h"
#include "remoteserver.h"
#include "smtpclient.h"
#include "summaryreport.h"
#include "updater.h"
#include "version.h"
#include "zipwriter.h"

#include <QMessageBox>
#include <QFileDialog>
#include <QInputDialog>
#include <QApplication>
#include <QCheckBox>
#include <QComboBox>
#include <QGroupBox>
#include <QPlainTextEdit>
#include <QRadioButton>
#include <QTimeEdit>
#include <QToolButton>
#include <QScroller>
#include <QThread>
#include <QMetaObject>
#include <QCloseEvent>
#include <QResizeEvent>
#include <QKeyEvent>
#include <QEvent>
#include <QTimer>
#include <QSplitter>
#include <QScrollBar>
#include <QGridLayout>
#include <QScrollArea>
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QLineEdit>
#include <QPushButton>
#include <QAbstractButton>
#include <QMouseEvent>
#include <QLabel>
#include <QSpinBox>
#include <QProgressBar>
#include <QMenu>
#include <QMenuBar>
#include <QAction>
#include <QLocale>
#include <unordered_map>
#include <QFrame>
#include <QFont>
#include <QSettings>
#include <QDateTime>
#include <QDir>
#include <QFile>
#include <QTextStream>
#include <QPointer>
#include <QFileInfo>
#include <QRandomGenerator>
#include <QProcess>
#include <QHostInfo>
#include <QTime>
#include <thread>
#include <chrono>
#include <sstream>
#include <iomanip>
#include <memory>
#include <map>
#include <algorithm>

// ── Currency-aware money formatting (UI side) ──────────────────────
static QString fmtMoney(double value)
{
    return Stats::formatMoney(value);
}

// ── Scanner burst sanity check ─────────────────────────────────────
// A barcode typed by a wedge scanner is a compact run with no spaces.
// Human-typed product names usually contain a space, so requiring
// "no whitespace" prevents a fast-typed phrase + Enter from being taken
// for a scan.
static bool looksLikeBarcode(const QString& s)
{
    if (s.size() < 2 || s.size() > 64) return false;
    for (const QChar& c : s) {
        if (c.isSpace()) return false;
    }
    return true;
}

// ── Local calendar helpers for sale timestamps ─────────────────────
// Stored timestamps are UTC ISO ("...T17:30:00Z"); convert to the user's
// local date/hour so "today", "yesterday" and the activity chart match
// what the shop actually experiences.
static QString saleDay(const Domain::Sale& s)
{
    return QString::fromStdString(Stats::localDay(s));
}

// ── Sell-keybind helpers ───────────────────────────────────────────
// Which top-level menu accelerators (Alt+<menu initial>) are taken? Those
// letters are dropped from the sell mapping, computed from the CURRENT
// (possibly translated) top-level menu titles.
static QList<int> menuAcceleratorKeys(QMainWindow* mw)
{
    QList<int> reserved;
    if (!mw || !mw->menuBar()) return reserved;
    const auto actions = mw->menuBar()->actions();
    for (QAction* a : actions) {
        if (!a->menu()) continue;
        QString t = a->menu()->title().trimmed();
        if (!t.isEmpty()) {
            reserved.append(static_cast<int>(t.at(0).toUpper().toLatin1()));
        }
    }
    return reserved;
}

// Map an Alt+key press to a sell-grid index (see sellKeyIndex below).
// This is the inverse: which Qt::Key sells the item at grid `index`.
// Index 0..9 → Alt+1..9 / Alt+0; beyond that the QWERTY letter rows,
// skipping reserved menu-accelerator letters.
static int sellKeyForIndex(int index, const QList<int>& reserved)
{
    if (index >= 0 && index < 10) return (index == 9) ? Qt::Key_0 : Qt::Key_1 + index;
    static const int letters[] = {
        Qt::Key_Q, Qt::Key_W, Qt::Key_E, Qt::Key_R, Qt::Key_T,
        Qt::Key_Y, Qt::Key_U, Qt::Key_I, Qt::Key_O, Qt::Key_P,
        Qt::Key_A, Qt::Key_S, Qt::Key_D, Qt::Key_F, Qt::Key_G,
        Qt::Key_H, Qt::Key_J, Qt::Key_K, Qt::Key_L,
        Qt::Key_Z, Qt::Key_X, Qt::Key_C, Qt::Key_V, Qt::Key_B,
        Qt::Key_N, Qt::Key_M
    };
    const int n = int(sizeof(letters) / sizeof(letters[0]));
    int used = 0;
    for (int i = 0; i < n; ++i) {
        if (reserved.contains(letters[i])) continue;
        if (letters[i] == Qt::Key_X) continue; // X = cancel sale, never a sell key
        if (used == index - 10) return letters[i];
        ++used;
    }
    return -1;
}

// "[ALT+1]" / "[ALT+Q]" hint text for a grid item, or "" when the item
// has no sell key (beyond the letter rows) or keybinds are disabled.
static QString sellKeyHint(int index, const QList<int>& reserved, bool enabled)
{
    if (!enabled) return QString();
    int k = sellKeyForIndex(index, reserved);
    if (k < 0) return QString();
    QString key;
    if (k >= Qt::Key_0 && k <= Qt::Key_9) key = QChar('0' + (k - Qt::Key_0));
    else key = QChar(k);
    return QStringLiteral("[ALT+") + key + QStringLiteral("]");
}

// ── Localized list-row text (UI side) ───────────────────────────────
// The data layer formats these rows in English ("Qty:", "Shelf:", ...);
// the UI rebuilds them through the translation layer so Polish users
// see Polish labels.
static QString displayStatusName(const std::string& code)
{
    if (Domain::statusIsInStock(code))    return Tr::trS("In Stock");
    if (Domain::statusIsLowStock(code))   return Tr::trS("Low Stock");
    if (Domain::statusIsOutOfStock(code)) return Tr::trS("Out of Stock");
    if (Domain::statusIsSoldOut(code))    return Tr::trS("Sold Out");
    return QString::fromStdString(code);
}

// Business-logic messages are English by design (a stable data-layer
// interface); translate them at the UI boundary. Two messages embed
// dynamic values (current stock level / required role) and are matched
// by prefix/suffix so the stored English text never leaks to the user.
static QString translatedBusinessMessage(const std::string& raw)
{
    const QString text = QString::fromStdString(raw);

    const QString stockPfx = QStringLiteral("Insufficient stock (available: ");
    if (text.startsWith(stockPfx) && text.endsWith(QLatin1Char(')'))) {
        const QString qty = text.mid(stockPfx.size(), text.size() - stockPfx.size() - 1);
        return Tr::trS("Insufficient stock (available: %1)").arg(qty);
    }

    const QString rolePfx = QStringLiteral("Access denied. Requires ");
    const QString roleSuf = QStringLiteral(" role or higher.");
    if (text.startsWith(rolePfx) && text.endsWith(roleSuf)) {
        const QString role = text.mid(rolePfx.size(),
                                      text.size() - rolePfx.size() - roleSuf.size());
        return Tr::trS("Access denied. Requires %1 role or higher.").arg(Tr::trS(role));
    }

    return Tr::trS(text);
}

static QString itemListText(const Domain::Item& it)
{
    return QString::fromStdString(it.name) + " | "
        + Tr::trS("Qty: ") + QString::number(it.quantity) + " | "
        + fmtMoney(it.price) + " | "
        + displayStatusName(it.status) + " | "
        + Tr::trS("Shelf: ") + QString::fromStdString(it.shelf) + " | "
        + Tr::trS("Category: ") + QString::fromStdString(it.category) + " | "
        + Tr::trS("ID: ") + QString::fromStdString(it.id);
}

// Rebuild a status combo: localized display text over language-
// independent numeric codes (item data), preserving the current
// selection by code. The codes are what gets stored/compared, so the
// UI language can be switched without touching any data.
static void rebuildStatusCombo(QComboBox* cb)
{
    if (!cb) return;
    const QVariant selected = cb->currentData();
    cb->clear();
    const struct StatusItem { const char* name; const char* code; } statuses[] = {
        { "In Stock", "0" }, { "Low Stock", "1" },
        { "Out of Stock", "2" }, { "Sold Out", "3" },
    };
    for (const auto& s : statuses) {
        cb->addItem(Tr::trS(QString::fromLatin1(s.name)), QString::fromLatin1(s.code));
    }
    if (selected.isValid()) {
        const int idx = cb->findData(selected);
        if (idx >= 0) cb->setCurrentIndex(idx);
    }
}

static QString saleListText(const Domain::Sale& s,
                            const std::unordered_map<std::string, std::string>& names)
{
    QString itemName = QString::fromStdString(
        names.count(s.itemId) ? names.at(s.itemId) : s.itemId);
    QString date = QString::fromStdString(Domain::toISOString(s.saleDate));
    date.replace('T', ' ');
    date.remove('Z');
    return Tr::trS("Sale ") + "#" + QString::fromStdString(s.id) + " | "
        + Tr::trS("Item: ") + itemName + " | "
        + Tr::trS("Qty: ") + QString::number(s.quantitySold) + " | "
        + fmtMoney(s.totalAmount) + " | "
        + Tr::trS("By: ") + QString::fromStdString(s.soldBy) + " | "
        + date;
}

// Compact "simple view" row for the Sales History list: item, quantity,
// total and date — no sale id / operator details.
static QString saleListTextSimple(const Domain::Sale& s,
                                  const std::unordered_map<std::string, std::string>& names)
{
    QString itemName = QString::fromStdString(
        names.count(s.itemId) ? names.at(s.itemId) : s.itemId);
    QString date = QString::fromStdString(Domain::toISOString(s.saleDate));
    date.replace('T', ' ');
    date.remove('Z');
    return itemName
        + " ×" + QString::number(s.quantitySold)
        + "  " + fmtMoney(s.totalAmount)
        + "  " + date;
}

MainWindow::MainWindow(DataAccess::IDataAccess& db, QWidget *parent)
    : QMainWindow(parent)
    , ui(new Ui::MainWindow)
    , m_db(db)
{
    ui->setupUi(this);

    // ── Window title ──────────────────────────────────────────
    setWindowTitle("QMark");

    // ── Load preferences ──────────────────────────────────────
    QSettings settings("QMark", "SchoolShop");
    bool telemetryEnabled = settings.value("telemetry/enabled", false).toBool();
    ui->chkTelemetry->setChecked(telemetryEnabled);
    if (telemetryEnabled) {
        AppLogger::instance().setTelemetryEnabled(true);
        QString ts = QDateTime::currentDateTime().toString("yyyyMMdd_hhmmss");
        QString telDir = QDir::currentPath() + "/telemetry";
        QDir().mkpath(telDir);
        QString logPath = telDir + "/telemetry_" + ts + ".log";
        QString dbPath  = telDir + "/telemetry_" + ts + ".db";
        AppLogger::instance().setLogFile(logPath);
        telemetry().open(dbPath);
        m_telemetryOn = true;
        // Record every key press and button click without hammering the
        // database: entries are buffered and flushed periodically.
        m_telemetryFlushTimer = new QTimer(this);
        connect(m_telemetryFlushTimer, &QTimer::timeout, this, [] { telemetry().flush(); });
        m_telemetryFlushTimer->start(2000);
    }

    bool worklogEnabled = settings.value("worklog/enabled", false).toBool();
    ui->chkWorklog->setChecked(worklogEnabled);
    m_worklog.setEnabled(worklogEnabled);

    // Language & currency (SuperAdmin-only switch in Preferences)
    QString language = settings.value("settings/language", "pl").toString();
    QString currency = settings.value("settings/currency", "PLN").toString();
    Domain::setCurrencySymbol(currency == "USD" ? "$" : "zł");
    if (ui->cboLanguage_pref) {
        ui->cboLanguage_pref->setItemData(0, "en");
        ui->cboLanguage_pref->setItemData(1, "pl");
        ui->cboLanguage_pref->setCurrentIndex(language == "pl" ? 1 : 0);
    }
    if (ui->cboCurrency_pref) {
        ui->cboCurrency_pref->setItemData(0, "PLN");
        ui->cboCurrency_pref->setItemData(1, "USD");
        ui->cboCurrency_pref->setCurrentIndex(currency == "USD" ? 1 : 0);
    }

    // Sell keybinds (toggleable by all roles, sell page + preferences)
    m_keybindsEnabled = settings.value("settings/sellKeybinds", true).toBool();
    if (ui->chkKeybinds_sell) ui->chkKeybinds_sell->setChecked(m_keybindsEnabled);
    if (ui->chkKeybinds_pref) ui->chkKeybinds_pref->setChecked(m_keybindsEnabled);

    // Connect dashboard buttons
    connect(ui->btnDashboardSell, &QPushButton::clicked, this, &MainWindow::on_btnDashboardSell_clicked);
    // Exit button in the top bar.
    connect(ui->btnExitNav, &QPushButton::clicked, this, &MainWindow::on_actionClose_triggered);

    // Dashboard clock: live time + date, refreshed every second.
    m_clockTimer = new QTimer(this);
    connect(m_clockTimer, &QTimer::timeout, this, &MainWindow::updateDashboardClock);
    m_clockTimer->start(1000);
    updateDashboardClock();

    // Report view: two resizable panes (report text | charts). The charts
    // pane itself is a vertical splitter so the pie and bar charts can be
    // resized individually.
    if (ui->splitReportBody) {
        ui->splitReportBody->setStretchFactor(0, 0);
        ui->splitReportBody->setStretchFactor(1, 1);
        ui->splitReportBody->setSizes({470, 640});
    }

    // Application-wide Alt+key interception: the sell-page keybinds must
    // keep working even while the search box (or any other sell-page
    // widget) holds focus.
    qApp->installEventFilter(this);

    // Sell-page search as you type (live card filtering). During an active
    // scanner burst the intermediate keystrokes are just part of one code,
    // so the grid rebuild is deferred until the burst ends or Enter seals
    // the scan (see applySellSearchFilter).
    if (ui->txtSearch_sell_page) {
        connect(ui->txtSearch_sell_page, &QLineEdit::textChanged, this, [this](const QString&) {
            applySellSearchFilter();
        });
    }

    // ── Sell-page grid takes all leftover vertical space ──────────
    if (ui->pageSellGrid && ui->scrollAreaItems && ui->pageSellGrid->layout()) {
        if (QBoxLayout* box = qobject_cast<QBoxLayout*>(ui->pageSellGrid->layout())) {
            box->setStretchFactor(ui->scrollAreaItems, 1);
        }
    }

    // ── Start at login page ───────────────────────────────────
    goToPage(0);

    // "Auto-generate ID" is checked by default: sync the Add Item form
    // (field disabled + a fresh candidate ID) with that state.
    if (ui->chkAutogenerateID_item && ui->chkAutogenerateID_item->isChecked()) {
        on_chkAutogenerateID_item_toggled(true);
    }

    // ── Build resupply page (appended to the stack) ───────────
    m_resupplyPage = buildResupplyPage();

    // ── Scanner mode state (settings-driven; timer for burst timing) ─
    m_scannerKeyTimer.start();
    m_scannerEnabled = AppSettings::scanEnabled();

    // Explain scanner behaviour right on the Add/Edit item pages.
    createScannerHintLabels();

    // Quick scanner toggles on the POS, Add/Edit/Remove item pages and
    // on the Automation settings page all share the same switch.
    setScannerMode(m_scannerEnabled);   // sync initial checked state
    auto wireScannerToggle = [this](QCheckBox* cb) {
        if (!cb) return;
        connect(cb, &QCheckBox::toggled, this, [this](bool on) {
            setScannerMode(on);
            showStatus(Tr::trS(on ? "Scanner mode enabled." : "Scanner mode disabled."), 3000);
        });
    };
    wireScannerToggle(ui->chkScanner_sell);
    wireScannerToggle(ui->chkScanner_item);
    wireScannerToggle(ui->chkScanner_item_edit);
    wireScannerToggle(ui->chkScanner_remove);

    // ── Build Automation & Remote page (appended after resupply) ─
    m_automationPage = buildAutomationPage();

    // ── Remote dashboard: bring up if enabled by the SuperAdmin ─
    startRemoteServerIfEnabled();

    // ── Scheduler: daily/monthly e-mail, scheduled backups, etc. ─
    m_schedulerTimer = new QTimer(this);
    connect(m_schedulerTimer, &QTimer::timeout, this, [this] { onSchedulerTick(); });
    m_schedulerTimer->start(30 * 1000);   // every 30 s; tasks fire on their minute

    // ── First-run: if no users exist, show setup form ─────────
    if (m_db.getAllUsers().empty()) {
        m_firstRunPage = buildFirstRunPage();
        if (m_firstRunPage && ui->stackedWidget) {
            ui->stackedWidget->setCurrentIndex(ui->stackedWidget->indexOf(m_firstRunPage));
        }
    }

    // ── Apply saved language, roles and currency labels ────────
    applyLanguageToUi();
    applyRoleRestrictions();
    updatePricePlaceholders();

    // ── Auto-update check (SuperAdmin-toggleable, on startup) ──
    if (AppSettings::updateEnabled()) {
        QTimer::singleShot(3 * 1000, this, [this] { checkUpdatesNow(); });
    }
}

MainWindow::~MainWindow()
{
    stopRemoteServer();
    if (m_schedulerTimer) m_schedulerTimer->stop();
    delete ui;
}

// ── First-run Setup (Form Page) ──────────────────────────────────
// Builds the first-run setup page programmatically and inserts it
// into the stacked widget. Shows on startup when no users exist in
// the database.
QWidget* MainWindow::buildFirstRunPage()
{
    QWidget* page = new QWidget();
    QVBoxLayout* mainLayout = new QVBoxLayout(page);
    mainLayout->setSpacing(0);

    // Top bar (matches login page style)
    QFrame* topBar = new QFrame();
    topBar->setMaximumHeight(60);
    topBar->setStyleSheet("background: #0078d4;");
    QHBoxLayout* topLayout = new QHBoxLayout(topBar);
    topLayout->setContentsMargins(15, 0, 15, 0);
    QLabel* title = new QLabel("QMark");
    title->setStyleSheet("color: white; font-size: 16px; font-weight: bold;");
    topLayout->addWidget(title);
    topLayout->addStretch();
    mainLayout->addWidget(topBar);

    // Center the form
    QVBoxLayout* centerWrapper = new QVBoxLayout();
    centerWrapper->addStretch();

    QHBoxLayout* centerRow = new QHBoxLayout();
    centerRow->addStretch();

    // Form frame
    QFrame* frame = new QFrame();
    frame->setMinimumWidth(400);
    frame->setMaximumWidth(500);
    frame->setFrameShape(QFrame::StyledPanel);
    frame->setStyleSheet(
        "QFrame { background: white; border: 1px solid #ddd; border-radius: 10px; }");
    QVBoxLayout* formLayout = new QVBoxLayout(frame);
    formLayout->setContentsMargins(30, 30, 30, 30);
    formLayout->setSpacing(12);

    // Title
    QLabel* formTitle = new QLabel("Welcome to QMark");
    formTitle->setAlignment(Qt::AlignCenter);
    QFont f = formTitle->font();
    f.setPointSize(20);
    f.setBold(true);
    formTitle->setFont(f);
    formLayout->addWidget(formTitle);

    QLabel* subtitle = new QLabel("No user accounts found.\nCreate a SuperAdmin account to get started.");
    subtitle->setAlignment(Qt::AlignCenter);
    subtitle->setStyleSheet("color: #666;");
    formLayout->addWidget(subtitle);

    formLayout->addSpacing(10);

    // Username
    QLineEdit* txtUser = new QLineEdit();
    txtUser->setPlaceholderText("Username");
    txtUser->setMinimumHeight(40);
    QFont inputFont;
    inputFont.setPointSize(13);
    txtUser->setFont(inputFont);
    formLayout->addWidget(txtUser);

    // Password
    QLineEdit* txtPass = new QLineEdit();
    txtPass->setPlaceholderText("Password");
    txtPass->setEchoMode(QLineEdit::Password);
    txtPass->setMinimumHeight(40);
    txtPass->setFont(inputFont);
    formLayout->addWidget(txtPass);

    // Confirm password
    QLineEdit* txtPass2 = new QLineEdit();
    txtPass2->setPlaceholderText("Confirm password");
    txtPass2->setEchoMode(QLineEdit::Password);
    txtPass2->setMinimumHeight(40);
    txtPass2->setFont(inputFont);
    formLayout->addWidget(txtPass2);

    // Status label (hidden by default)
    QLabel* statusLabel = new QLabel();
    statusLabel->setAlignment(Qt::AlignCenter);
    statusLabel->setStyleSheet("color: #c62828;");
    statusLabel->hide();
    formLayout->addWidget(statusLabel);

    formLayout->addSpacing(5);

    // Create Account button
    QPushButton* btnCreate = new QPushButton("CREATE ACCOUNT");
    btnCreate->setMinimumHeight(50);
    QFont btnFont;
    btnFont.setPointSize(14);
    btnFont.setBold(true);
    btnCreate->setFont(btnFont);
    btnCreate->setStyleSheet(
        "QPushButton { background-color: #0078d4; color: white; border: none; border-radius: 8px; }"
        "QPushButton:hover { background-color: #005a9e; }");
    btnCreate->setCursor(Qt::PointingHandCursor);
    formLayout->addWidget(btnCreate);

    centerRow->addWidget(frame);
    centerRow->addStretch();

    centerWrapper->addLayout(centerRow);
    centerWrapper->addStretch();
    mainLayout->addLayout(centerWrapper);

    // Insert page into stacked widget
    ui->stackedWidget->addWidget(page);

    // Connect the button
    connect(btnCreate, &QPushButton::clicked, this, [this, txtUser, txtPass, txtPass2, statusLabel]() {
        QString username = txtUser->text().trimmed();
        QString password1 = txtPass->text();
        QString password2 = txtPass2->text();

        // Validate
        if (username.isEmpty() || password1.isEmpty() || password2.isEmpty()) {
            statusLabel->setText(Tr::trS("All fields are required."));
            statusLabel->show();
            return;
        }

        if (password1 != password2) {
            statusLabel->setText(Tr::trS("Passwords do not match."));
            statusLabel->show();
            return;
        }

        if (password1.length() < 4) {
            statusLabel->setText(Tr::trS("Password must be at least 4 characters."));
            statusLabel->show();
            return;
        }

        if (m_db.getUserByUsername(username.toStdString()).has_value()) {
            statusLabel->setText(Tr::trS("Username already exists."));
            statusLabel->show();
            return;
        }

        // Create the SuperAdmin
        std::string hashedPw = hash_string(password1.toStdString());
        if (hashedPw.empty()) {
            statusLabel->setText(Tr::trS("Failed to hash password. Try again."));
            statusLabel->show();
            return;
        }

        Domain::User admin;
        admin.id = std::to_string(std::chrono::duration_cast<std::chrono::milliseconds>(
            std::chrono::system_clock::now().time_since_epoch()).count());
        admin.username = username.toStdString();
        admin.passwordHash = hashedPw;
        admin.role = Domain::User::Role::SuperAdmin;
        admin.createdAt = Domain::now();

        if (m_db.addUser(admin)) {
            LOG_INFO("First-run: SuperAdmin account created: " + username);
            QMessageBox::information(this, Tr::trS("Setup Complete"),
                Tr::trS("SuperAdmin account created!\n\nYou can now log in."));
            // Navigate to login page
            goToPage(0);
        } else {
            statusLabel->setText(Tr::trS("Failed to create user. Try again."));
            statusLabel->show();
        }
    });

    return page;
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
    // Auto-adapt grid columns when window resizes (on sell page)
    if (ui->gridLayoutItemScroll && ui->scrollAreaItems) {
        int width = ui->scrollAreaItems->viewport()->width();
        int cols = qMax(1, width / 260); // each card ~240px + 20px margin
        for (int _c = 0; _c < cols; ++_c) ui->gridLayoutItemScroll->setColumnStretch(_c, 1);
    }
}

// ═══════════════════════════════════════════════════════════════════
// Navigation / roles / language / currency helpers
// ═══════════════════════════════════════════════════════════════════

// Central navigation: clerks may only visit the login page (0) and the
// sell page (6). All other roles navigate freely.
void MainWindow::goToPage(int index)
{
    if (!ui->stackedWidget) return;
    if (index < 0 || index >= ui->stackedWidget->count()) return;

    // Leaving a scan-page mid-burst (menu navigation, login, ...) must not
    // carry a half-collected code into the next page.
    resetScannerBurst();

    if (m_isLoggedIn && getCurrentUserRole() == Domain::User::Role::UserRole) {
        if (index != 0 && index != 6) return; // clerk: blocked
    }

    // No leftover credentials: clear the form fields every time the
    // login / register pages are (re)loaded.
    if (index == 0) { // login page
        if (ui->txtUsername_login) ui->txtUsername_login->clear();
        if (ui->txtPassword_login) {
            ui->txtPassword_login->clear();
            ui->txtPassword_login->setEchoMode(QLineEdit::Password);
        }
        if (ui->chkHide_login)     ui->chkHide_login->setChecked(false);
    } else if (index == 5 && m_isLoggedIn) { // undo removed page: auto-populate
        refreshRemovedItemsList();
    } else if (index == 16) { // register page
        if (ui->txtUsername_register)  ui->txtUsername_register->clear();
        if (ui->txtPassword1_register) {
            ui->txtPassword1_register->clear();
            ui->txtPassword1_register->setEchoMode(QLineEdit::Password);
        }
        if (ui->txtPassword2_register) {
            ui->txtPassword2_register->clear();
            ui->txtPassword2_register->setEchoMode(QLineEdit::Password);
        }
        if (ui->chkHide_register)      ui->chkHide_register->setChecked(false);
        on_txtPassword1_register_textChanged(QString()); // reset strength bar
    }

    ui->stackedWidget->setCurrentIndex(index);
}

// Hide/restore menu items depending on the current user's role.
// Clerks only keep: Sell menu + Account menu (Log Out / Exit only).
// NOTE: visibility must be toggled via menuAction()->setVisible(), NOT
// menu->setVisible(): QMenu is a Qt::Popup widget, so calling
// QMenu::setVisible(true) POPS THE MENU OPEN instead of just showing
// the menu-bar button (that's why every menu used to auto-expand on
// login). menuAction()->setVisible() toggles the button only.
void MainWindow::applyRoleRestrictions()
{
    bool clerk = m_isLoggedIn && getCurrentUserRole() == Domain::User::Role::UserRole;
    bool superAdmin = m_isLoggedIn && getCurrentUserRole() == Domain::User::Role::SuperAdmin;

    if (ui->menuItems) ui->menuItems->menuAction()->setVisible(!clerk);
    if (ui->menuSales) ui->menuSales->menuAction()->setVisible(!clerk);
    if (ui->menuTools) ui->menuTools->menuAction()->setVisible(!clerk);
    if (ui->menuSell) ui->menuSell->menuAction()->setVisible(true);
    if (ui->menuLanguage) ui->menuLanguage->menuAction()->setVisible(true); // all roles
    if (ui->menuFile) {
        if (ui->actionLog_in) ui->actionLog_in->setVisible(!clerk);
        if (ui->actionRegister) ui->actionRegister->setVisible(!clerk);
    }
    // The automation & remote settings page is SuperAdmin-only.
    if (ui->actionAutomation) {
        ui->actionAutomation->setVisible(superAdmin);
    }
}

// Retranslate the whole window (menus, actions, static widgets and the
// dynamically built resupply/first-run pages).
void MainWindow::applyLanguageToUi()
{
    if (ui->centralwidget) Tr::applyLanguage(ui->centralwidget);
    if (menuBar()) Tr::applyLanguage(menuBar());
    if (m_resupplyPage) Tr::applyLanguage(m_resupplyPage);
    if (m_firstRunPage) Tr::applyLanguage(m_firstRunPage);
    if (m_automationPage) Tr::applyLanguage(m_automationPage);
    updatePricePlaceholders();
    syncLanguageUi();
    // Dashboard calendar follows the UI language, too.
    if (ui->calendarDashboard) {
        ui->calendarDashboard->setLocale((Tr::language() == "pl")
            ? QLocale(QLocale::Polish) : QLocale(QLocale::English));
    }
    // Keep the (optional) POS simple-view statistics in the new language.
    if (ui->frameStats_sell && ui->frameStats_sell->isVisible()) refreshSellStats();
}

// Keep the Language menu (and the Preferences language combo) in sync
// with the currently active language, without re-triggering handlers.
void MainWindow::syncLanguageUi()
{
    QString lang = Tr::language();
    if (ui->actionLanguageEnglish) {
        ui->actionLanguageEnglish->blockSignals(true);
        ui->actionLanguageEnglish->setChecked(lang != "pl");
        ui->actionLanguageEnglish->blockSignals(false);
    }
    if (ui->actionLanguagePolish) {
        ui->actionLanguagePolish->blockSignals(true);
        ui->actionLanguagePolish->setChecked(lang == "pl");
        ui->actionLanguagePolish->blockSignals(false);
    }
    if (ui->cboLanguage_pref) {
        ui->cboLanguage_pref->blockSignals(true);
        ui->cboLanguage_pref->setCurrentIndex(lang == "pl" ? 1 : 0);
        ui->cboLanguage_pref->blockSignals(false);
    }
    // Role names in the registration combo are UI labels, so translate
    // them from the canonical English values (indexes stay the same).
    if (ui->cboRole_register) {
        const QString canonical[3] = { "Clerk", "Admin", "SuperAdmin" };
        for (int i = 0; i < 3 && i < ui->cboRole_register->count(); ++i) {
            ui->cboRole_register->setItemText(i, Tr::trS(canonical[i]));
        }
    }
    // Status combo boxes: localized display names over language-
    // independent numeric codes, so switching language never touches data.
    rebuildStatusCombo(ui->cboStatus_item);
    rebuildStatusCombo(ui->cboStatus_item_edit);
    // Search-field combo on the Edit page: localized labels over the
    // canonical field codes used by the SQL queries.
    if (ui->cboSearchField_item_edit) {
        const QString canonical[5] = { "All", "Name", "Category", "Shelf", "Status" };
        for (int i = 0; i < 5 && i < ui->cboSearchField_item_edit->count(); ++i) {
            ui->cboSearchField_item_edit->setItemData(i, canonical[i].toLower());
            ui->cboSearchField_item_edit->setItemText(i, Tr::trS(canonical[i]));
        }
    }
}

// ── Language menu ─────────────────────────────────────────────────
// A quick way to switch between English and Polish (all roles).
void MainWindow::on_actionLanguageEnglish_triggered()
{
    QSettings settings("QMark", "SchoolShop");
    settings.setValue("settings/language", "en");
    applyLanguageToUi();
    statusBar()->showMessage(Tr::trS("Language set to English"), 3000);
}

void MainWindow::on_actionLanguagePolish_triggered()
{
    QSettings settings("QMark", "SchoolShop");
    settings.setValue("settings/language", "pl");
    applyLanguageToUi();
    statusBar()->showMessage(Tr::trS("Language set to Polish"), 3000);
}

// Keep the price field placeholders in sync with the active currency.
void MainWindow::updatePricePlaceholders()
{
    QString ph = (Domain::currencySymbol() == "zł")
        ? Tr::trS("Price (zł) *")
        : Tr::trS("Price ($) *");
    if (ui->txtPrice_item) ui->txtPrice_item->setPlaceholderText(ph);
    if (ui->txtPrice_item_edit) ui->txtPrice_item_edit->setPlaceholderText(ph);
}

// ═══════════════════════════════════════════════════════════════════
// Login / Register
// ═══════════════════════════════════════════════════════════════════

bool MainWindow::isLoggedIn() const { return m_isLoggedIn; }
void MainWindow::setLoggedIn(bool loggedIn) { m_isLoggedIn = loggedIn; }

bool MainWindow::checkLoginRequired(bool)
{
    if (!m_isLoggedIn) {
        QMessageBox::information(this, Tr::trS("Login Required"), Tr::trS("Please log in first."));
        return false;
    }
    return true;
}

bool MainWindow::checkRoleRequired(BusinessLogic::RequiredRole required, bool)
{
    if (!m_isLoggedIn) {
        QMessageBox::information(this, Tr::trS("Login Required"), Tr::trS("Please log in first."));
        return false;
    }
    auto check = BusinessLogic::checkUserRole(m_currentUser, required);
    if (!check.hasAccess) {
        QMessageBox::warning(this, Tr::trS("Access Denied"), translatedBusinessMessage(check.errorMessage));
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
        QMessageBox::warning(this, Tr::trS("Login"), Tr::trS("Please enter username and password."));
        return;
    }

    std::optional<Domain::User> user = BusinessLogic::login(
        m_db, username.toStdString(), password.toStdString());

    if (!user.has_value()) {
        QMessageBox::warning(this, Tr::trS("Login"), Tr::trS("Invalid username or password."));
        return;
    }

    setCurrentUser(user.value());
    setLoggedIn(true);

    LOG_INFO("User logged in: " + username);

    // Record who is operating so worklog/telemetry entries carry the user.
    QString uname = QString::fromStdString(user->username);
    QString role  = QString::fromStdString(user->roleName());
    m_worklog.setUser(uname.toStdString(), role.toStdString());
    AppLogger::instance().setUser(uname, role);
    telemetry().setUser(uname, role);

    applyRoleRestrictions();

    // Clerks land directly on the sell page (only menu they can use)
    if (getCurrentUserRole() == Domain::User::Role::UserRole) {
        goToPage(6); // sell page
        refreshSellPage();
    } else {
        goToPage(1); // dashboard
        refreshDashboard();
    }

    statusBar()->showMessage(Tr::trS("Logged in as ") + username
        + " (" + QString::fromStdString(user->roleName()) + ")");
}

// ── Register ───────────────────────────────────────────────────
void MainWindow::on_btnRegister_clicked()
{
    // Allow registration without login when no users exist (first-run)
    bool noUsers = m_db.getAllUsers().empty();
    if (!noUsers && (!m_isLoggedIn || getCurrentUserRole() != Domain::User::Role::SuperAdmin)) {
        QMessageBox::warning(this, Tr::trS("Register"), Tr::trS("Only SuperAdmin can register new users."));
        return;
    }

    QString username = ui->txtUsername_register->text().trimmed();
    QString password1 = ui->txtPassword1_register->text();
    QString password2 = ui->txtPassword2_register->text();

    if (password1 != password2) {
        QMessageBox::warning(this, Tr::trS("Register"), Tr::trS("Passwords do not match."));
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
    // BusinessLogic::addUser() hashes the raw password before storing;
    // do NOT pre-hash here (double hashing would break login).
    userDto.password = password1.toStdString();
    userDto.role = role;

    auto result = BusinessLogic::addUser(m_db, userDto);
    if (result.isValid) {
        QMessageBox::information(this, Tr::trS("Register"), Tr::trS("User registered successfully."));
        LOG_INFO("User registered: " + username);
    } else {
        QMessageBox::warning(this, Tr::trS("Register"), translatedBusinessMessage(result.errorMessage));
    }
}

void MainWindow::on_btnClear_username_register_clicked() { ui->txtUsername_register->clear(); }
void MainWindow::on_btnClear_password1_register_clicked() { ui->txtPassword1_register->clear(); }
void MainWindow::on_btnClear_password2_register_clicked() { ui->txtPassword2_register->clear(); }

void MainWindow::on_btnHelp_role_register_clicked()
{
    QString info;
    info += Tr::trS("Roles and their permissions:") + "\n\n";
    info += "• " + Tr::trS("Clerk") + " — " + Tr::trS("Clerk role: can sell items, view the sales log and undo sales only.") + "\n";
    info += "• " + Tr::trS("Admin") + " — " + Tr::trS("Admin role: manages items, shelves, categories, prices, reports and worklogs.") + "\n";
    info += "• " + Tr::trS("SuperAdmin") + " — " + Tr::trS("SuperAdmin role: everything an Admin can do, plus user accounts, roles, passwords and currency.") + "\n";
    QMessageBox::information(this, Tr::trS("Role Info"), info);
}

void MainWindow::on_chkHide_login_toggled(bool checked)
{
    // The checkbox is labelled "Show password": checked → visible.
    ui->txtPassword_login->setEchoMode(checked ? QLineEdit::Normal : QLineEdit::Password);
}

void MainWindow::on_chkHide_register_toggled(bool checked)
{
    ui->txtPassword1_register->setEchoMode(checked ? QLineEdit::Normal : QLineEdit::Password);
    ui->txtPassword2_register->setEchoMode(checked ? QLineEdit::Normal : QLineEdit::Password);
}

void MainWindow::on_txtPassword1_register_textChanged(const QString& text)
{
    // Password strength meter: 0..4 based on length, case, digits, symbols.
    int score = 0;
    if (text.length() >= 8)  score++;           // long enough
    if (text.length() >= 12) score++;           // very long
    bool hasUpper = false, hasLower = false, hasDigit = false, hasSymbol = false;
    for (const QChar& c : text) {
        if (c.isUpper()) hasUpper = true;
        else if (c.isLower()) hasLower = true;
        else if (c.isDigit()) hasDigit = true;
        else hasSymbol = true;
    }
    if (hasUpper && hasLower) score++;          // mixed case
    if (hasDigit)             score++;          // contains a digit
    if (hasSymbol)            score++;          // contains a symbol
    if (score > 4) score = 4;
    if (text.isEmpty()) score = 0;

    if (ui->pwdStrength_register) {
        ui->pwdStrength_register->setValue(score);
        const char* chunkColor = score <= 1 ? "#c62828" :   // red
                                 score == 2 ? "#ef6c00" :   // orange
                                 score == 3 ? "#7cb342" :   // light green
                                             "#2e7d32";     // green
        ui->pwdStrength_register->setStyleSheet(
            QString("QProgressBar { border: 1px solid #cccccc; border-radius: 7px; "
                    "background: #eeeeee; } "
                    "QProgressBar::chunk { background-color: %1; border-radius: 7px; }")
                .arg(chunkColor));
    }
    if (ui->lblPwdStrength_register) {
        QString label;
        if (text.isEmpty())      label = Tr::trS("Password strength");
        else if (score <= 1)     label = Tr::trS("Weak");
        else if (score == 2)     label = Tr::trS("Medium");
        else if (score == 3)     label = Tr::trS("Strong");
        else                     label = Tr::trS("Very strong");
        ui->lblPwdStrength_register->setText(label);
    }
}

// ═══════════════════════════════════════════════════════════════════
// Menu Actions
// ═══════════════════════════════════════════════════════════════════

void MainWindow::on_actionClose_triggered() { close(); }

void MainWindow::on_actionLog_out_triggered()
{
    LOG_INFO("User logged out: " + QString::fromStdString(m_currentUser.value_or(Domain::User{}).username));
    m_worklog.clearUser();
    AppLogger::instance().clearUser();
    telemetry().clearUser();
    clearCurrentUser();
    applyRoleRestrictions();
    goToPage(0);
    statusBar()->showMessage(Tr::trS("Logged out"));
}

void MainWindow::on_actionLog_in_triggered()
{
    goToPage(0);
}

void MainWindow::on_actionRegister_triggered()
{
    bool noUsers = m_db.getAllUsers().empty();
    if (noUsers || checkRoleRequired(BusinessLogic::RequiredRole::SuperAdmin)) {
        goToPage(16); // Register page
    }
}

// ═══════════════════════════════════════════════════════════════════
// Items — Add
// ═══════════════════════════════════════════════════════════════════

void MainWindow::on_actionAdd_Items_triggered()
{
    if (!checkRoleRequired(BusinessLogic::RequiredRole::Admin)) return;
    goToPage(2); // Add Item page
}

void MainWindow::on_btnAdd_item_clicked()
{
    if (!checkRoleRequired(BusinessLogic::RequiredRole::Admin)) return;

    QString name = ui->txtName_item->text().trimmed();
    QString quantity = ui->txtQuantity_item->text().trimmed();
    QString price = ui->txtPrice_item->text().trimmed();
    QString category = ui->txtCategory_item->text().trimmed();
    QString shelf = ui->txtShelf_item->text().trimmed();
    QString status = ui->cboStatus_item
        ? ui->cboStatus_item->currentData().toString()
        : QString::fromStdString(Domain::statusInStock());
    if (status.isEmpty()) status = QString::fromStdString(Domain::statusInStock());
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
        QMessageBox::information(this, Tr::trS("Add Item"), Tr::trS("Item added successfully."));
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
        QMessageBox::warning(this, Tr::trS("Add Item"), translatedBusinessMessage(result.errorMessage));
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
        QMessageBox::information(this, Tr::trS("Check ID"), Tr::trS("Enter an ID to check."));
        return;
    }
    bool exists = m_db.checkIdExists("items", id.toStdString());
    QMessageBox::information(this, Tr::trS("Check ID"), exists ? Tr::trS("ID already exists.") : Tr::trS("ID is available."));
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
    if (!checkRoleRequired(BusinessLogic::RequiredRole::Admin)) return;
    goToPage(3); // Edit Item page
    on_btnSearch_item_edit_clicked();   // autopopulate the list
}

void MainWindow::on_btnSearch_item_edit_clicked()
{
    QString term = ui->txtSearch_item_edit->text().trimmed();
    QString field = ui->cboSearchField_item_edit
        ? ui->cboSearchField_item_edit->currentData().toString() : "";
    auto list = BusinessLogic::populateList(m_db, "items", term.toStdString(), field.toStdString());
    auto all = m_db.getAllItems();
    std::unordered_map<std::string, Domain::Item> byId;
    for (const auto& it : all) byId[it.id] = it;
    ui->lstSearch_item_edit->clear();
    for (const auto& entry : list) {
        QString text = byId.count(entry.id) ? itemListText(byId[entry.id])
                                            : QString::fromStdString(entry.displayText);
        QListWidgetItem* lwi = new QListWidgetItem(text);
        lwi->setData(Qt::UserRole, QString::fromStdString(entry.id));
        ui->lstSearch_item_edit->addItem(lwi);
    }
}

void MainWindow::on_lstSearch_item_edit_itemClicked(QListWidgetItem *item)
{
    loadItemToEdit(item->data(Qt::UserRole).toString());
}

// Fills the Edit Item form from an item id (shared by the list click
// handler and the barcode-scanner flow). Optionally selects the row in
// the search list when the item is present.
void MainWindow::loadItemToEdit(const QString& id)
{
    auto itemOpt = m_db.getItemById(id.toStdString());
    if (!itemOpt.has_value()) return;
    ui->txtName_item_edit->setText(QString::fromStdString(itemOpt->name));
    ui->txtQuantity_item_edit->setText(QString::number(itemOpt->quantity));
    ui->txtPrice_item_edit->setText(QString::number(itemOpt->price, 'f', 2));
    ui->txtCategory_item_edit->setText(QString::fromStdString(itemOpt->category));
    ui->txtShelf_item_edit->setText(QString::fromStdString(itemOpt->shelf));
    ui->txtId_item_edit->setText(QString::fromStdString(itemOpt->id));
    if (ui->cboStatus_item_edit) {
        int idx = ui->cboStatus_item_edit->findData(QString::fromStdString(itemOpt->status));
        if (idx < 0) {
            idx = ui->cboStatus_item_edit->findText(QString::fromStdString(itemOpt->status));
        }
        if (idx >= 0) ui->cboStatus_item_edit->setCurrentIndex(idx);
    }
    // Highlight the matching row in the results list, if present.
    if (ui->lstSearch_item_edit) {
        for (int i = 0; i < ui->lstSearch_item_edit->count(); ++i) {
            QListWidgetItem* lwi = ui->lstSearch_item_edit->item(i);
            if (lwi->data(Qt::UserRole).toString() == id) {
                ui->lstSearch_item_edit->setCurrentRow(i);
                break;
            }
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
    itemDto.status = ui->cboStatus_item_edit
        ? ui->cboStatus_item_edit->currentData().toString().toStdString()
        : Domain::statusInStock();
    if (itemDto.status.empty()) itemDto.status = Domain::statusInStock();
    itemDto.createdAt = origCreatedAt;
    itemDto.updatedAt = Domain::toISOString(Domain::now());

    auto result = BusinessLogic::updateItem(m_db, itemDto);
    if (result.isValid) {
        QMessageBox::information(this, Tr::trS("Edit Item"), Tr::trS("Item updated successfully."));
        LOG_INFO("Item updated: " + QString::fromStdString(itemDto.name));
        m_worklog.logEntry(WorklogEntry::ActionType::Edit, WorklogEntry::EntityType::Item,
                          itemDto.id, "Edited item: " + itemDto.name);
    } else {
        QMessageBox::warning(this, Tr::trS("Edit Item"), translatedBusinessMessage(result.errorMessage));
    }
}

void MainWindow::on_btnClear_name_item_edit_clicked() { ui->txtName_item_edit->clear(); }
void MainWindow::on_btnClear_id_item_edit_clicked() { ui->txtId_item_edit->clear(); }

// ═══════════════════════════════════════════════════════════════════
// Items — Remove
// ═══════════════════════════════════════════════════════════════════

void MainWindow::on_actionRemove_Items_triggered()
{
    if (!checkRoleRequired(BusinessLogic::RequiredRole::Admin)) return;
    goToPage(4); // Remove Item page
    on_btnSearch_item_remove_clicked(); // autopopulate the list
}

void MainWindow::on_btnSearch_item_remove_clicked()
{
    QString term = ui->txtSearch_item_remove->text().trimmed();
    auto list = BusinessLogic::populateList(m_db, "items", term.toStdString(), "");
    auto all = m_db.getAllItems();
    std::unordered_map<std::string, Domain::Item> byId;
    for (const auto& it : all) byId[it.id] = it;
    ui->lstSearch_item_remove->clear();
    for (const auto& entry : list) {
        QString text = byId.count(entry.id) ? itemListText(byId[entry.id])
                                            : QString::fromStdString(entry.displayText);
        QListWidgetItem* lwi = new QListWidgetItem(text);
        lwi->setData(Qt::UserRole, QString::fromStdString(entry.id));
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

    if (QMessageBox::question(this, Tr::trS("Confirm Remove"),
        Tr::trS("Remove item \"%1\"?").arg(name)) == QMessageBox::Yes) {
        if (m_db.removeItem(id.toStdString())) {
            QMessageBox::information(this, Tr::trS("Remove Item"), Tr::trS("Item removed successfully."));
            LOG_INFO("Item removed: " + name);
            m_worklog.logEntry(WorklogEntry::ActionType::Remove, WorklogEntry::EntityType::Item,
                              id.toStdString(), "Removed item: " + name.toStdString());
        } else {
            QMessageBox::warning(this, Tr::trS("Remove Item"), Tr::trS("Failed to remove item."));
        }
    }
}

void MainWindow::on_btnUndoRemove_item_clicked()
{
    if (!checkRoleRequired(BusinessLogic::RequiredRole::Admin)) return;
    goToPage(5); // Undo removed page
    refreshRemovedItemsList(); // never show an empty page
}

void MainWindow::on_btnUndoLast_item_clicked()
{
    if (!checkRoleRequired(BusinessLogic::RequiredRole::Admin)) return;
    auto removed = m_db.getRemovedItems();
    if (!removed.empty()) {
        QString id = QString::fromStdString(removed.front().id);
        if (m_db.restoreItem(id.toStdString())) {
            QMessageBox::information(this, Tr::trS("Undo Remove"), Tr::trS("Item restored."));
        }
    }
}

// ═══════════════════════════════════════════════════════════════════
// Undo — Removed Items
// ═══════════════════════════════════════════════════════════════════

// (Re)populate the Undo Removed Items list. Called from both the menu
// action and the Remove-Item page button, and automatically whenever the
// user lands on the page (see goToPage) so it is never left empty.
void MainWindow::refreshRemovedItemsList()
{
    if (!ui->lstSearch_undoremoved) return;
    ui->lstSearch_undoremoved->clear();
    auto removed = m_db.getRemovedItems();
    for (const auto& item : removed) {
        QListWidgetItem* lwi = new QListWidgetItem(itemListText(item));
        lwi->setData(Qt::UserRole, QString::fromStdString(item.id));
        ui->lstSearch_undoremoved->addItem(lwi);
    }
}

void MainWindow::on_actionUndo_Removed_Items_triggered()
{
    if (!checkRoleRequired(BusinessLogic::RequiredRole::Admin)) return;
    goToPage(5);
    refreshRemovedItemsList();
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
            QMessageBox::information(this, Tr::trS("Undo"), Tr::trS("Item restored."));
            refreshRemovedItemsList();
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
    QMessageBox::information(this, Tr::trS("Undo All"),
        Tr::trS("%1 item(s) restored.").arg(count));
    refreshRemovedItemsList();
}

// ═══════════════════════════════════════════════════════════════════
// Dashboard
// ═══════════════════════════════════════════════════════════════════

void MainWindow::refreshDashboard()
{
    // Total items
    auto items = m_db.getAllItems();
    int totalItems = static_cast<int>(items.size());
    if (ui->lblTotalItemsValue)
        ui->lblTotalItemsValue->setText(QString::number(totalItems));

    // Build item name lookup
    std::unordered_map<std::string, std::string> itemNames;
    for (const auto& it : items) {
        itemNames[it.id] = it.name;
    }

    // Sales stats
    auto sales = m_db.getAllSales();
    int itemsSold = 0;
    double revenue = 0.0;
    for (const auto& s : sales) {
        itemsSold += s.quantitySold;
        revenue += s.totalAmount;
    }
    if (ui->lblItemsSoldValue)
        ui->lblItemsSoldValue->setText(QString::number(itemsSold));
    if (ui->lblRevenueValue)
        ui->lblRevenueValue->setText(fmtMoney(revenue));

    // Recent sales (last 10) — getAllSales returns most recent first
    if (ui->lstRecentSales) {
        ui->lstRecentSales->clear();
        int count = 0;
        for (int i = 0; i < static_cast<int>(sales.size()) && count < 10; ++i, ++count) {
            const auto& s = sales[i];
            QString itemName = QString::fromStdString(
                itemNames.count(s.itemId) ? itemNames[s.itemId] : s.itemId);
            QString text = itemName
                + "  ×" + QString::number(s.quantitySold)
                + "  " + fmtMoney(s.totalAmount);
            QListWidgetItem* item = new QListWidgetItem(text);
            item->setData(Qt::UserRole, QString::fromStdString(s.id));
            ui->lstRecentSales->addItem(item);
        }
        if (ui->lstRecentSales->count() == 0) {
            QListWidgetItem* placeholder = new QListWidgetItem(Tr::trS("No sales recorded yet."));
            placeholder->setFlags(Qt::NoItemFlags);
            ui->lstRecentSales->addItem(placeholder);
        }
    }
}

// Live dashboard clock (updates every second via m_clockTimer).
// The date respects the selected UI language: Polish displays Polish
// weekday/month names, English the English ones.
void MainWindow::updateDashboardClock()
{
    QDateTime now = QDateTime::currentDateTime();
    if (ui->lblClockTime)
        ui->lblClockTime->setText(now.toString("HH:mm:ss"));
    if (ui->lblClockDate) {
        QLocale loc = (Tr::language() == "pl") ? QLocale(QLocale::Polish)
                                                : QLocale(QLocale::English);
        ui->lblClockDate->setText(loc.toString(now.date(), "dddd, d MMMM yyyy"));
    }
}

void MainWindow::on_btnDashboardSell_clicked()
{
    on_actionSell_Item_triggered();
}

void MainWindow::on_lstRecentSales_itemClicked(QListWidgetItem *item)
{
    Q_UNUSED(item);
    // Could show sale details in the future
}

// ═══════════════════════════════════════════════════════════════════
// Resupply
// ═══════════════════════════════════════════════════════════════════

void MainWindow::on_actionResupply_triggered()
{
    if (!checkRoleRequired(BusinessLogic::RequiredRole::Admin)) return;
    if (m_resupplyPage && ui->stackedWidget) {
        goToPage(ui->stackedWidget->indexOf(m_resupplyPage));
    }
}

QWidget* MainWindow::buildResupplyPage()
{
    QWidget* page = new QWidget();
    page->setObjectName("resupplyPage");
    QVBoxLayout* mainLayout = new QVBoxLayout(page);
    mainLayout->setSpacing(0);

    // ── Top bar ──
    QFrame* topBar = new QFrame();
    topBar->setMaximumHeight(60);
    topBar->setStyleSheet("background: #0078d4;");
    QHBoxLayout* topLayout = new QHBoxLayout(topBar);
    topLayout->setContentsMargins(15, 0, 15, 0);

    QPushButton* btnBack = new QPushButton(Tr::trS("← Back"));
    btnBack->setStyleSheet(
        "QPushButton { color: white; background: transparent; border: none; "
        "font-size: 14px; font-weight: bold; padding: 5px 10px; }"
        "QPushButton:hover { background: rgba(255,255,255,0.15); border-radius: 4px; }");
    btnBack->setCursor(Qt::PointingHandCursor);
    btnBack->setMinimumHeight(36);
    topLayout->addWidget(btnBack);

    QLabel* title = new QLabel(Tr::trS("Resupply"));
    title->setStyleSheet("color: white; font-size: 16px; font-weight: bold;");
    topLayout->addWidget(title);
    topLayout->addStretch();
    mainLayout->addWidget(topBar);

    // ── Content ──
    QHBoxLayout* contentLayout = new QHBoxLayout();
    contentLayout->setContentsMargins(15, 15, 15, 15);
    contentLayout->setSpacing(20);

    // ── Left panel: search + results ──
    QWidget* leftPanel = new QWidget();
    QVBoxLayout* leftLayout = new QVBoxLayout(leftPanel);
    leftLayout->setContentsMargins(0, 0, 0, 0);

    QLabel* searchLabel = new QLabel(Tr::trS("Search Items"));
    QFont searchFont = searchLabel->font();
    searchFont.setPointSize(14);
    searchFont.setBold(true);
    searchLabel->setFont(searchFont);
    leftLayout->addWidget(searchLabel);

    // Search bar
    QLineEdit* searchInput = new QLineEdit();
    searchInput->setObjectName("resupplySearch");
    searchInput->setPlaceholderText(Tr::trS("Type item name to search..."));
    searchInput->setMinimumHeight(40);
    QFont searchInputFont;
    searchInputFont.setPointSize(13);
    searchInput->setFont(searchInputFont);
    searchInput->setStyleSheet(
        "QLineEdit { border: 2px solid #ddd; border-radius: 8px; padding: 8px 12px; }"
        "QLineEdit:focus { border-color: #0078d4; }");
    leftLayout->addWidget(searchInput);

    // Results list
    QListWidget* resultsList = new QListWidget();
    resultsList->setObjectName("resupplyResults");
    resultsList->setMinimumHeight(300);
    resultsList->setStyleSheet(
        "QListWidget { border: 1px solid #ddd; border-radius: 8px; background: white; }"
        "QListWidget::item { padding: 10px; border-bottom: 1px solid #eee; }"
        "QListWidget::item:selected { background: #e3f2fd; color: black; }"
        "QListWidget::item:hover { background: #f5f5f5; }");
    leftLayout->addWidget(resultsList);

    contentLayout->addWidget(leftPanel, 1);

    // ── Right panel: selected item + quick quantity controls ──
    QWidget* rightPanel = new QWidget();
    rightPanel->setMinimumWidth(320);
    QVBoxLayout* rightLayout = new QVBoxLayout(rightPanel);
    rightLayout->setContentsMargins(0, 0, 0, 0);

    QLabel* selectedLabel = new QLabel(Tr::trS("Selected Item"));
    selectedLabel->setFont(searchFont);
    rightLayout->addWidget(selectedLabel);

    // Item info card
    QFrame* itemCard = new QFrame();
    itemCard->setFrameShape(QFrame::StyledPanel);
    itemCard->setStyleSheet(
        "QFrame { background: white; border: 1px solid #ddd; border-radius: 10px; padding: 15px; }"
        "QLabel { background: transparent; }");
    QVBoxLayout* cardLayout = new QVBoxLayout(itemCard);

    QLabel* itemNameLabel = new QLabel(Tr::trS("No item selected"));
    itemNameLabel->setObjectName("resupplyItemName");
    QFont nameFont = itemNameLabel->font();
    nameFont.setPointSize(16);
    nameFont.setBold(true);
    itemNameLabel->setFont(nameFont);
    itemNameLabel->setStyleSheet("color: black; background: transparent;");
    cardLayout->addWidget(itemNameLabel);

    QLabel* itemInfoLabel = new QLabel(Tr::trS("Search and select an item from the list"));
    itemInfoLabel->setObjectName("resupplyItemInfo");
    itemInfoLabel->setStyleSheet("color: #666; background: transparent;");
    cardLayout->addWidget(itemInfoLabel);

    QLabel* qtyLabel = new QLabel("");
    qtyLabel->setObjectName("resupplyQty");
    QFont qtyFont = qtyLabel->font();
    qtyFont.setPointSize(14);
    qtyFont.setBold(true);
    qtyLabel->setFont(qtyFont);
    qtyLabel->setStyleSheet("color: #0078d4; background: transparent;");
    cardLayout->addWidget(qtyLabel);

    rightLayout->addWidget(itemCard);
    rightLayout->addSpacing(15);

    // ── Quick add/subtract buttons (no manual editing) ──
    QLabel* quickLabel = new QLabel(Tr::trS("Adjust quantity:"));
    quickLabel->setStyleSheet("font-weight: bold; font-size: 13px;");
    rightLayout->addWidget(quickLabel);

    QGridLayout* quickGrid = new QGridLayout();
    quickGrid->setSpacing(8);
    const int deltas[6] = { -10, -5, -1, 1, 5, 10 };
    QVector<QPushButton*> quickBtns;
    for (int i = 0; i < 6; ++i) {
        // Positive buttons show "+N" (matching the "-N" removal buttons)
        QString label = (deltas[i] > 0)
            ? QString("+%1").arg(deltas[i])
            : QString::number(deltas[i]);
        QPushButton* b = new QPushButton(label);
        b->setMinimumHeight(46);
        b->setCursor(Qt::PointingHandCursor);
        QFont f = b->font();
        f.setPointSize(14);
        f.setBold(true);
        b->setFont(f);
        if (deltas[i] > 0) {
            b->setStyleSheet(
                "QPushButton { background-color: #2e7d32; color: white; border: none; border-radius: 8px; }"
                "QPushButton:hover { background-color: #1b5e20; }");
        } else {
            b->setStyleSheet(
                "QPushButton { background-color: #e53935; color: white; border: none; border-radius: 8px; }"
                "QPushButton:hover { background-color: #b71c1c; }");
        }
        b->setObjectName(QString("resupplyQuick_%1").arg(i));
        quickBtns.append(b);
        quickGrid->addWidget(b, i / 3, i % 3);
    }
    rightLayout->addLayout(quickGrid);
    rightLayout->addSpacing(8);

    // ── Fine control: spin box (optional) + apply ──
    QHBoxLayout* spinRow = new QHBoxLayout();
    QSpinBox* qtySpinBox = new QSpinBox();
    qtySpinBox->setObjectName("resupplySpin");
    qtySpinBox->setRange(0, 999999);
    qtySpinBox->setValue(0);
    qtySpinBox->setMinimumHeight(45);
    QFont spinFont;
    spinFont.setPointSize(16);
    spinFont.setBold(true);
    qtySpinBox->setFont(spinFont);
    qtySpinBox->setStyleSheet(
        "QSpinBox { border: 2px solid #ddd; border-radius: 8px; padding: 5px 10px; color: black; }"
        "QSpinBox:focus { border-color: #0078d4; }");
    spinRow->addWidget(qtySpinBox);

    QPushButton* btnApply = new QPushButton(Tr::trS("APPLY"));
    btnApply->setObjectName("resupplyApply");
    btnApply->setMinimumHeight(45);
    btnApply->setCursor(Qt::PointingHandCursor);
    QFont applyFont = btnApply->font();
    applyFont.setPointSize(14);
    applyFont.setBold(true);
    btnApply->setFont(applyFont);
    btnApply->setStyleSheet(
        "QPushButton { background-color: #0078d4; color: white; border: none; border-radius: 8px; padding: 0 20px; }"
        "QPushButton:hover { background-color: #005a9e; }"
        "QPushButton:disabled { background-color: #ccc; color: #666; }");
    btnApply->setEnabled(false);
    spinRow->addWidget(btnApply);
    rightLayout->addLayout(spinRow);

    // Status
    QLabel* statusLabel = new QLabel("");
    statusLabel->setObjectName("resupplyStatus");
    statusLabel->setAlignment(Qt::AlignCenter);
    statusLabel->setWordWrap(true);
    rightLayout->addWidget(statusLabel);

    rightLayout->addStretch();

    contentLayout->addWidget(rightPanel, 1);

    mainLayout->addLayout(contentLayout);

    // Append page to the stack and remember it for navigation
    ui->stackedWidget->addWidget(page);

    // ── State ──
    // Heap-allocated so every deferred lambda (clicked/textChanged/
    // currentChanged connections) still OWNS it after this function
    // returns. A stack local captured by reference here would dangle and
    // crash the app on the first click (use-after-free).
    auto selectedItemId = std::make_shared<QString>();

    // (Re)populates the results list for the current search term. An
    // empty term lists everything, so the page starts fully populated.
    auto refillList = [this, resultsList, searchInput]() {
        QString term = searchInput->text().trimmed();
        resultsList->clear();
        auto items = m_db.getAllItems();
        for (const auto& item : items) {
            QString name = QString::fromStdString(item.name);
            if (!term.isEmpty() && !name.contains(term, Qt::CaseInsensitive)) continue;
            QListWidgetItem* lwi = new QListWidgetItem(
                name + "  (" + Tr::trS("Qty: ") + QString::number(item.quantity) + ")");
            lwi->setData(Qt::UserRole, QString::fromStdString(item.id));
            resultsList->addItem(lwi);
        }
    };

    // Sets the quantity, persists it, and refreshes the UI.
    auto applyQuantity = [this, selectedItemId, qtySpinBox, qtyLabel, statusLabel,
                          resultsList, itemInfoLabel](int newQty, const QString& verb) {
        if (selectedItemId->isEmpty()) {
            statusLabel->setText(Tr::trS("Item not found."));
            statusLabel->setStyleSheet("color: #c62828;");
            return;
        }
        auto itemOpt = m_db.getItemById(selectedItemId->toStdString());
        if (!itemOpt.has_value()) {
            statusLabel->setText(Tr::trS("Item not found."));
            statusLabel->setStyleSheet("color: #c62828;");
            return;
        }
        if (newQty < 0) newQty = 0;

        Domain::Item updated = itemOpt.value();
        updated.quantity = newQty;
        // Auto-update status
        if (newQty == 0) updated.status = Domain::statusOutOfStock();
        else if (newQty <= 5) updated.status = Domain::statusLowStock();
        else updated.status = Domain::statusInStock();
        updated.updatedAt = Domain::now();

        if (m_db.updateItem(updated)) {
            qtyLabel->setText(Tr::trS("Current: ") + QString::number(newQty));
            statusLabel->setText(verb + " " + QString::number(newQty) + "!");
            statusLabel->setStyleSheet("color: #2e7d32; font-weight: bold;");
            qtySpinBox->setValue(newQty);
            // Update the search result row so it stays in sync
            for (int i = 0; i < resultsList->count(); ++i) {
                QListWidgetItem* lwi = resultsList->item(i);
                if (lwi && lwi->data(Qt::UserRole).toString() == *selectedItemId) {
                    lwi->setText(QString::fromStdString(updated.name)
                                 + "  (" + Tr::trS("Qty: ") + QString::number(newQty) + ")");
                }
            }
            itemInfoLabel->setText(Tr::trS("Category: ") + QString::fromStdString(updated.category)
                                   + "  |  " + Tr::trS("Shelf: ") + QString::fromStdString(updated.shelf)
                                   + "  |  " + fmtMoney(updated.price));
            m_worklog.logEntry(WorklogEntry::ActionType::Edit, WorklogEntry::EntityType::Item,
                              selectedItemId->toStdString(),
                              "Resupply: adjusted quantity to " + std::to_string(newQty));
        } else {
            statusLabel->setText(Tr::trS("Failed to update. Try again."));
            statusLabel->setStyleSheet("color: #c62828;");
        }
    };

    // Refreshes the right panel with the currently selected item.
    auto refreshSelection = [this, selectedItemId, itemNameLabel, itemInfoLabel, qtyLabel,
                             qtySpinBox, btnApply, resultsList]() {
        QListWidgetItem* current = resultsList->currentItem();
        if (!current) return;
        QString id = current->data(Qt::UserRole).toString();
        auto itemOpt = m_db.getItemById(id.toStdString());
        if (!itemOpt.has_value()) return;
        const auto& item = itemOpt.value();
        *selectedItemId = id;
        itemNameLabel->setText(QString::fromStdString(item.name));
        itemInfoLabel->setText(Tr::trS("Category: ") + QString::fromStdString(item.category)
                               + "  |  " + Tr::trS("Shelf: ") + QString::fromStdString(item.shelf)
                               + "  |  " + fmtMoney(item.price));
        qtyLabel->setText(Tr::trS("Current: ") + QString::number(item.quantity));
        qtySpinBox->setValue(item.quantity);
        btnApply->setEnabled(true);
    };

    // ── Connections ──

    // Back button
    connect(btnBack, &QPushButton::clicked, this, [this]() {
        goToPage(1); // dashboard
    });

    // Search as you type
    connect(searchInput, &QLineEdit::textChanged, this, [this, refillList]() {
        refillList();
    });

    // Select item from the list
    connect(resultsList, &QListWidget::itemClicked, this,
        [this, selectedItemId, refreshSelection](QListWidgetItem* current) {
            if (!current) return;
            *selectedItemId = current->data(Qt::UserRole).toString();
            refreshSelection();
        });

    // Quick +/− buttons: adjust and save immediately (no manual editing)
    for (int i = 0; i < 6; ++i) {
        int delta = deltas[i];
        connect(quickBtns[i], &QPushButton::clicked, this,
            [this, selectedItemId, delta, qtySpinBox, applyQuantity]() {
                if (selectedItemId->isEmpty()) return;
                auto itemOpt = m_db.getItemById(selectedItemId->toStdString());
                if (!itemOpt.has_value()) return;
                int newQty = itemOpt->quantity + delta;
                if (newQty < 0) newQty = 0;
                applyQuantity(newQty, Tr::trS("Quantity updated to"));
            });
    }

    // Apply button (uses the spin box value)
    connect(btnApply, &QPushButton::clicked, this,
        [this, selectedItemId, qtySpinBox, applyQuantity]() {
            if (selectedItemId->isEmpty()) return;
            applyQuantity(qtySpinBox->value(), Tr::trS("Quantity set to"));
        });

    // Re-select the highlighted item when the page is shown again
    connect(ui->stackedWidget, &QStackedWidget::currentChanged, this,
        [this, page, refillList, refreshSelection]() {
            if (ui->stackedWidget->currentWidget() == page) {
                refillList();      // keep the list fresh / populated
                refreshSelection();
            }
        });

    // The page starts fully populated (no search needed)
    refillList();

    return page;
}

// ═══════════════════════════════════════════════════════════════════
// Sell Item (POS)
// ═══════════════════════════════════════════════════════════════════

void MainWindow::on_actionSell_Item_triggered()
{
    if (!checkLoginRequired()) return;
    goToPage(6); // POS page
    refreshSellPage();
}

void MainWindow::refreshSellPage()
{
    auto items = m_db.getAllItems();
    rebuildItemGrid(items);
    refreshSalesLog();
    refreshSellStats();
}

// Live card filtering for the POS search box. Skipped while a scanner
// burst is mid-flight (the intermediate characters are all part of one
// code); it runs again when the burst ends or aborts, so the grid always
// catches up with what the user actually typed.
void MainWindow::applySellSearchFilter()
{
    if (!ui->txtSearch_sell_page) return;
    if (!ui->stackedWidget || ui->stackedWidget->currentIndex() != 6) return;
    if (m_scannerInBurst) return;
    const QString term = ui->txtSearch_sell_page->text().trimmed();
    if (term.isEmpty()) rebuildItemGrid(m_db.getAllItems());
    else rebuildItemGrid(m_db.searchItems(term.toStdString(), ""));
}

// Simple-view statistics for the POS page: today's totals.
// All aggregation is delegated to the GUI-independent Stats engine.
void MainWindow::refreshSellStats()
{
    if (!ui->frameStats_sell) return;
    if (!ui->frameStats_sell->isVisible()) return; // hidden unless simple view on

    auto sales  = m_db.getAllSales();
    auto items  = m_db.getAllItems();

    std::unordered_map<std::string, std::string> itemNames;
    for (const auto& it : items) itemNames[it.id] = it.name;
    std::unordered_map<std::string, std::string> userNames;
    for (const auto& u : m_db.getAllUsers()) userNames[u.id] = u.username;

    auto snap = Stats::compute(sales, itemNames, userNames);

    // Compact per-product summary of today's sales. Instead of one line
    // per transaction (which makes the stats panel grow unboundedly during
    // a busy day), aggregate quantity and revenue per product and cap the
    // list, so the panel keeps a stable, readable size.
    struct SoldRun { int qty = 0; double revenue = 0.0; };
    std::vector<std::pair<QString, SoldRun>> soldBy;      // first-seen order
    QString today = QDate::currentDate().toString("yyyy-MM-dd");
    for (const auto& s : sales) {
        if (saleDay(s) != today) continue;
        QString name = QString::fromStdString(itemNames.count(s.itemId) ? itemNames[s.itemId] : s.itemId);
        bool found = false;
        for (auto& p : soldBy) {
            if (p.first == name) {
                p.second.qty += s.quantitySold;
                p.second.revenue += s.totalAmount;
                found = true;
                break;
            }
        }
        if (!found) soldBy.push_back({ name, { s.quantitySold, s.totalAmount } });
    }

    QStringList soldLines;
    const int kMaxSoldLines = 15;
    int shown = 0;
    for (const auto& p : soldBy) {
        if (++shown > kMaxSoldLines) continue;
        QString name = p.first;
        if (p.second.qty > 1) name = QString::number(p.second.qty) + "× " + name;
        soldLines << name + "  " + fmtMoney(p.second.revenue);
    }
    const int more = static_cast<int>(soldBy.size()) - kMaxSoldLines;
    if (more > 0) soldLines << Tr::trS("… and %1 more").arg(more);

    ui->lblStatSales_sell->setText(Tr::trS("Sales: ") + QString::number(snap.todayTx));
    ui->lblStatRevenue_sell->setText(Tr::trS("Revenue: ") + fmtMoney(snap.todayRevenue));
    ui->lblStatItems_sell->setText(Tr::trS("Items sold: ") + QString::number(snap.todayItems));
    if (ui->lblSoldLines_sell) {
        ui->lblSoldLines_sell->setText(soldLines.isEmpty()
            ? Tr::trS("No sales today.")
            : soldLines.join("\n"));
    }
}

void MainWindow::on_chkSimpleView_sell_toggled(bool checked)
{
    bool simple = checked && ui->chkSimpleView_sell && ui->chkSimpleView_sell->isChecked();
    if (ui->frameSalesLog_sell) ui->frameSalesLog_sell->setVisible(!simple);
    if (ui->frameStats_sell)    ui->frameStats_sell->setVisible(simple);
    if (simple) refreshSellStats();
}

void MainWindow::on_btnSearch_sell_page_clicked()
{
    QString term = ui->txtSearch_sell_page->text().trimmed();
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

    m_sellItems = items;
    m_sellCards.clear();
    m_sellHighlightIndex = -1;

    // Clear existing grid
    QLayoutItem* child;
    while ((child = ui->gridLayoutItemScroll->takeAt(0)) != nullptr) {
        if (child->widget()) child->widget()->deleteLater();
        delete child;
    }

    int width = ui->scrollAreaItems->viewport()->width();
    int cols = qMax(1, width / 260);
    for (int _c = 0; _c < cols; ++_c) ui->gridLayoutItemScroll->setColumnStretch(_c, 1);

    int row = 0, col = 0;
    const QList<int> reserved = menuAcceleratorKeys(this);
    for (int i = 0; i < static_cast<int>(items.size()); ++i) {
        QWidget* card = createItemCard(items[i], sellKeyHint(i, reserved, m_keybindsEnabled));
        ui->gridLayoutItemScroll->addWidget(card, row, col);
        m_sellCards.append(card);
        // Wire the card's SELL button to the shared sell-by-index flow
        QPushButton* sellBtn = card->findChild<QPushButton*>("sellCardBtn");
        if (sellBtn) {
            int index = i;
            connect(sellBtn, &QPushButton::clicked, this, [this, index]() {
                sellProductAtIndex(index);
            });
        }
        col++;
        if (col >= cols) { col = 0; row++; }
    }
}

QWidget* MainWindow::createItemCard(const Domain::Item& item, const QString& keyHint)
{
    QFrame* card = new QFrame();
    card->setFrameShape(QFrame::StyledPanel);
    card->setMinimumSize(210, 150);
    card->setStyleSheet(
        "QFrame { background: white; border: 2px solid #ddd; border-radius: 10px; margin: 5px; }"
        "QFrame:hover { border-color: #0078d4; }"
    );

    QVBoxLayout* layout = new QVBoxLayout(card);

    // Header row: item name on the left, the Alt+<key> sell shortcut
    // badge in the TOP-RIGHT corner of the card.
    QWidget* header = new QWidget(card);
    QHBoxLayout* headerLayout = new QHBoxLayout(header);
    headerLayout->setContentsMargins(0, 0, 0, 0);
    headerLayout->setSpacing(6);

    // Item name — explicit color keeps the text visible on light cards
    // (fixes white/grey-on-white text on Windows 11).
    QLabel* nameLbl = new QLabel(QString::fromStdString(item.name));
    QFont nameFont = nameLbl->font();
    nameFont.setPointSize(12);
    nameFont.setBold(true);
    nameLbl->setFont(nameFont);
    nameLbl->setWordWrap(true);
    nameLbl->setStyleSheet("color: black; background: transparent;");
    headerLayout->addWidget(nameLbl, 1);

    if (!keyHint.isEmpty()) {
        QLabel* keyLbl = new QLabel(keyHint);
        QFont keyFont = keyLbl->font();
        keyFont.setPointSize(10);
        keyFont.setBold(true);
        keyLbl->setFont(keyFont);
        keyLbl->setAlignment(Qt::AlignCenter);
        keyLbl->setStyleSheet(
            "QLabel { background-color: #0078d4; color: white; border-radius: 6px; "
            "padding: 2px 8px; font-size: 11px; font-weight: bold; }");
        headerLayout->addWidget(keyLbl, 0, Qt::AlignTop);
    }
    layout->addWidget(header);

    // Price
    QLabel* priceLbl = new QLabel(fmtMoney(item.price));
    QFont priceFont = priceLbl->font();
    priceFont.setPointSize(15);
    priceFont.setBold(true);
    priceLbl->setFont(priceFont);
    priceLbl->setStyleSheet("color: #2e7d32; background: transparent;");
    layout->addWidget(priceLbl);

    // Quantity & status
    QString statusColor = "#2e7d32";
    if (Domain::statusIsLowStock(item.status)) statusColor = "#f57f17";
    else if (Domain::statusIsOutOfStock(item.status)) statusColor = "#c62828";

    QLabel* qtyLbl = new QLabel(Tr::trS("Qty: ") + QString::number(item.quantity) +
                                 "  |  " + displayStatusName(item.status));
    qtyLbl->setStyleSheet("color: " + statusColor + "; font-size: 11px; background: transparent;");
    layout->addWidget(qtyLbl);

    // Shelf/Category
    if (!item.shelf.empty() || !item.category.empty()) {
        QString info = QString::fromStdString(item.shelf);
        if (!item.category.empty()) info += "  |  " + QString::fromStdString(item.category);
        QLabel* infoLbl = new QLabel(info);
        infoLbl->setStyleSheet("color: #666; font-size: 11px; background: transparent;");
        layout->addWidget(infoLbl);
    }

    layout->addStretch();

    // Sell button — large touch target
    QPushButton* sellBtn = new QPushButton(Tr::trS("SELL"));
    sellBtn->setObjectName("sellCardBtn");
    sellBtn->setMinimumHeight(34);
    sellBtn->setCursor(Qt::PointingHandCursor);
    sellBtn->setStyleSheet(
        "QPushButton { background-color: #0078d4; color: white; font-size: 14px; "
        "font-weight: bold; border: none; border-radius: 8px; padding: 6px; }"
        "QPushButton:hover { background-color: #005a9e; }"
        "QPushButton:pressed { background-color: #003f7f; }"
        "QPushButton:disabled { background-color: #ccc; color: #666; }"
    );
    sellBtn->setEnabled(item.quantity > 0 && !Domain::statusIsSoldOut(item.status));
    layout->addWidget(sellBtn);

    return card;
}

// Highlight (and scroll to) the card at `index`. -1 clears the highlight.
void MainWindow::highlightSellCard(int index)
{
    auto resetBorder = [](QWidget* w) {
        if (auto* f = qobject_cast<QFrame*>(w)) {
            f->setStyleSheet(
                "QFrame { background: white; border: 2px solid #ddd; border-radius: 10px; margin: 5px; }"
                "QFrame:hover { border-color: #0078d4; }");
        }
    };
    auto highlightBorder = [](QWidget* w) {
        if (auto* f = qobject_cast<QFrame*>(w)) {
            f->setStyleSheet(
                "QFrame { background: #fff8e1; border: 3px solid #f57f17; border-radius: 10px; margin: 5px; }");
        }
    };

    if (m_sellHighlightIndex >= 0 && m_sellHighlightIndex < m_sellCards.size()) {
        resetBorder(m_sellCards[m_sellHighlightIndex]);
    }
    m_sellHighlightIndex = -1;

    if (index >= 0 && index < m_sellCards.size()) {
        m_sellHighlightIndex = index;
        highlightBorder(m_sellCards[index]);
        if (ui->scrollAreaItems) {
            ui->scrollAreaItems->ensureWidgetVisible(m_sellCards[index]);
        }
    }
}

// Sells ONE unit of the product at the given grid index. Quick POS
// interface: no quantity dialog, no confirmation popup — the sale just
// happens and the grid/log refresh. Returns true when a sale happened.
bool MainWindow::sellProductAtIndex(int index)
{
    if (index < 0 || index >= static_cast<int>(m_sellItems.size())) return false;

    const auto& item = m_sellItems[index];
    if (item.quantity <= 0 || Domain::statusIsSoldOut(item.status)) {
        QMessageBox::information(this, Tr::trS("Sell Item"), Tr::trS("This item is out of stock."));
        return false;
    }

    highlightSellCard(index);
    m_selectedSellItemId = QString::fromStdString(item.id);

    auto result = BusinessLogic::sellItem(
        m_db, item.id, 1, m_currentUser.value_or(Domain::User{}).id);
    if (result.isValid) {
        m_worklog.logEntry(WorklogEntry::ActionType::Sale, WorklogEntry::EntityType::Sale,
                           item.id, "Sold item: " + item.name);
        refreshSellPage();
        refreshDashboard();
        return true;
    }

    QMessageBox::warning(this, Tr::trS("Sale Error"), translatedBusinessMessage(result.errorMessage));
    return false;
}

// ── Sales log (sell page) ──────────────────────────────────────────

void MainWindow::refreshSalesLog()
{
    if (!ui->lstSalesLog_sell) return;

    auto sales = m_db.getAllSales();        // newest first
    auto items = m_db.getAllItems();
    auto removed = m_db.getRemovedItems();

    std::unordered_map<std::string, std::string> names;
    for (const auto& it : items) names[it.id] = it.name;
    for (const auto& r : removed) names[r.id] = r.name;   // deleted items too

    ui->lstSalesLog_sell->clear();
    for (const auto& s : sales) {
        QString itemName = QString::fromStdString(names.count(s.itemId) ? names[s.itemId] : s.itemId);
        QString date = QString::fromStdString(Domain::toISOString(s.saleDate));
        date.replace(QString("T"), QString(" "));
        date.remove(QString("Z"));
        QString text = "[" + date + "] " + itemName
            + " ×" + QString::number(s.quantitySold)
            + "  " + fmtMoney(s.totalAmount)
            + "  #" + QString::fromStdString(s.id);
        QListWidgetItem* lwi = new QListWidgetItem(text);
        lwi->setData(Qt::UserRole, QString::fromStdString(s.id));
        ui->lstSalesLog_sell->addItem(lwi);
    }
    if (sales.empty()) {
        QListWidgetItem* ph = new QListWidgetItem(Tr::trS("No sales recorded yet."));
        ph->setFlags(Qt::NoItemFlags);
        ui->lstSalesLog_sell->addItem(ph);
    }
}

void MainWindow::on_btnRefreshSalesLog_sell_clicked()
{
    refreshSalesLog();
}

void MainWindow::on_btnUndoSale_sell_clicked()
{
    if (!checkLoginRequired()) return;

    auto sales = m_db.getAllSales();
    if (sales.empty()) {
        QMessageBox::information(this, Tr::trS("Undo Sale"), Tr::trS("No sales to undo."));
        return;
    }

    QString saleId;
    QListWidgetItem* sel = ui->lstSalesLog_sell ? ui->lstSalesLog_sell->currentItem() : nullptr;
    if (sel && (sel->flags() & Qt::ItemIsEnabled)) {
        saleId = sel->data(Qt::UserRole).toString();
    }
    // No selection → undo the most recent sale
    if (saleId.isEmpty()) saleId = QString::fromStdString(sales[0].id);

    undoSaleById(saleId);
}

// Generalized undo: works for ANY sale (selected or most recent). Stock
// is restored — the item is even restored from removed_items if it was
// deleted after the sale — then the sale record is deleted.
void MainWindow::undoSaleById(const QString& saleId)
{
    if (saleId.isEmpty()) return;

    auto sales = m_db.getAllSales();
    std::optional<Domain::Sale> saleFound;
    for (const auto& s : sales) {
        if (QString::fromStdString(s.id) == saleId) { saleFound = s; break; }
    }
    if (!saleFound.has_value()) {
        QMessageBox::warning(this, Tr::trS("Undo Sale"), Tr::trS("Sale not found."));
        return;
    }
    const auto& sale = *saleFound;

    auto item = m_db.getItemById(sale.itemId);
    QString itemName;
    if (item.has_value()) {
        itemName = QString::fromStdString(item->name);
    } else {
        // Item may have been deleted after the sale — look in removed_items
        for (const auto& r : m_db.getRemovedItems()) {
            if (r.id == sale.itemId) { itemName = QString::fromStdString(r.name); break; }
        }
        if (itemName.isEmpty()) itemName = QString::fromStdString(sale.itemId);
    }

    // No confirmation dialog (quick interface): undo happens directly.
    // Only failures show a message.

    // Restore the item first if it had been removed
    if (!item.has_value()) {
        bool restoredItem = false;
        for (const auto& r : m_db.getRemovedItems()) {
            if (r.id == sale.itemId) { restoredItem = m_db.restoreItem(r.id); break; }
        }
        if (!restoredItem) {
            QMessageBox::warning(this, Tr::trS("Undo Sale"),
                Tr::trS("The item was removed and could not be restored."));
            return;
        }
        item = m_db.getItemById(sale.itemId);
    }

    if (item.has_value()) {
        Domain::Item updated = item.value();
        updated.quantity += sale.quantitySold;
        // Refresh status (keep it consistent with the new quantity)
        if (updated.quantity > 0 && Domain::statusIsOutOfStock(updated.status)) {
            updated.status = updated.quantity <= 5 ? Domain::statusLowStock() : Domain::statusInStock();
        } else if (updated.quantity > 10 && Domain::statusIsLowStock(updated.status)) {
            updated.status = Domain::statusInStock();
        }
        updated.updatedAt = Domain::now();
        m_db.updateItem(updated);
    }

    m_db.deleteSale(sale.id);

    m_worklog.logEntry(WorklogEntry::ActionType::Undo, WorklogEntry::EntityType::Sale,
                       sale.id, "Sale undone: " + itemName.toStdString()
                       + " x" + std::to_string(sale.quantitySold));
    LOG_INFO("Sale undone: " + itemName + " x" + QString::number(sale.quantitySold));
    refreshSellPage();
    refreshDashboard();
}

// ── Sell keybinds ───────────────────────────────────────────────────

void MainWindow::on_chkKeybinds_sell_toggled(bool checked)
{
    m_keybindsEnabled = checked;
    QSettings settings("QMark", "SchoolShop");
    settings.setValue("settings/sellKeybinds", checked);
    if (ui->chkKeybinds_pref) {
        ui->chkKeybinds_pref->blockSignals(true);
        ui->chkKeybinds_pref->setChecked(checked);
        ui->chkKeybinds_pref->blockSignals(false);
    }
    // The Alt+<key> badges on the item cards depend on this flag.
    if (m_isLoggedIn && ui->stackedWidget
        && ui->stackedWidget->currentIndex() == 6) {
        refreshSellPage();
    }
}

// Map an Alt+key press to a sell-grid index. The first 10 items use
// Alt+1..9 / Alt+0; beyond that the keys go down the QWERTY rows
// (q w e r t y u i o p a s d f g h j k l z x c v b n m). Any letter
// whose Alt-combo is already used elsewhere (top-level menu accelerators
// such as Alt+<menu initial>) is removed from the mapping, exactly as
// requested - so those combos keep their normal function. Returns -1
// when the key is not a sell key.
static int sellKeyIndex(int key, const QList<int>& reserved)
{
    if (key >= Qt::Key_1 && key <= Qt::Key_9) return key - Qt::Key_1;
    if (key == Qt::Key_0) return 9;
    if (reserved.contains(key)) return -1;
    if (key == Qt::Key_X) return -1; // Alt+X cancels the last sale, never sells

    static const int letters[] = {
        Qt::Key_Q, Qt::Key_W, Qt::Key_E, Qt::Key_R, Qt::Key_T,
        Qt::Key_Y, Qt::Key_U, Qt::Key_I, Qt::Key_O, Qt::Key_P,
        Qt::Key_A, Qt::Key_S, Qt::Key_D, Qt::Key_F, Qt::Key_G,
        Qt::Key_H, Qt::Key_J, Qt::Key_K, Qt::Key_L,
        Qt::Key_Z, Qt::Key_X, Qt::Key_C, Qt::Key_V, Qt::Key_B,
        Qt::Key_N, Qt::Key_M
    };
    const int n = int(sizeof(letters) / sizeof(letters[0]));
    int used = 0;                      // non-reserved letters seen so far
    for (int i = 0; i < n; ++i) {
        if (letters[i] == key) return 10 + used;
        if (letters[i] == Qt::Key_X) continue; // reserved for cancel-sale
        if (!reserved.contains(letters[i])) ++used;
    }
    return -1;
}

// Application-wide key interceptor. Because the sell-page Alt+key combos
// must keep working even when the search box (or any other widget on the
// sell page) has focus — key events to a focused child widget never reach
// MainWindow::keyPressEvent — the Alt+key presses are consumed here first.
// Ctrl is excluded so AltGr-typed characters on non-US layouts still land
// in text fields, and letters reserved by the menu bar are left alone
// (they return -1 from sellKeyIndex and fall through to the menus).
bool MainWindow::eventFilter(QObject* watched, QEvent* event)
{
    // A mid-flight scanner burst ends when the operator clicks somewhere:
    // the typed characters were never consumed, so the card grid has to
    // catch up with what is currently in the search box.
    if (m_scannerInBurst && event->type() == QEvent::MouseButtonPress) {
        resetScannerBurst();
        applySellSearchFilter();
    }

    // ── Telemetry: record every key press and every button click ──
    if (m_telemetryOn) {
        if (event->type() == QEvent::KeyPress) {
            auto* kev = static_cast<QKeyEvent*>(event);
            QString text = kev->text();
            int keyCode = kev->key();
            // Never record actual password characters (neither the text nor
            // a keycode that would reveal them on most keyboard layouts).
            if (auto* le = qobject_cast<QLineEdit*>(QApplication::focusWidget())) {
                if (le->echoMode() != QLineEdit::Normal) {
                    text = QStringLiteral("***");
                    keyCode = -1;
                }
            }
            const QString where = watched
                ? QString::fromLatin1(watched->metaObject()->className()) + QLatin1Char('/')
                  + watched->objectName()
                : QStringLiteral("-");
            telemetry().write("KEY", QString("key=0x%1 text='%2' mods=%3 widget=%4")
                .arg(keyCode, 0, 16).arg(text).arg(int(kev->modifiers())).arg(where));
        } else if (event->type() == QEvent::MouseButtonRelease) {
            auto* mev = static_cast<QMouseEvent*>(event);
            auto* btn = qobject_cast<QAbstractButton*>(watched);
            if (btn && mev->button() == Qt::LeftButton) {
                const QString name = btn->objectName().isEmpty() ? btn->text() : btn->objectName();
                telemetry().write("CLICK", name);
            }
        }
    }

    // ── Scanner mode (barcode scanner = keyboard wedge) ─────────────
    // A USB/HID scanner "types" the code very fast (all characters within
    // ~50 ms) and terminates with Enter. We detect such bursts on the POS,
    // Add Item and Edit Item pages and act on the collected code. The
    // detector NEVER consumes printable characters: every key reaches the
    // focused widget exactly like manual typing, so a scanner connection
    // can never break the keyboard. Only the Enter that seals a confirmed
    // burst is consumed.
    if (event->type() == QEvent::KeyPress
        && watched != this
        && m_scannerEnabled
        && m_isLoggedIn
        && !QApplication::activeModalWidget()
        && ui->stackedWidget) {
        const int curPage = ui->stackedWidget->currentIndex();
        const bool scanPage = (curPage == 6)      // POS sell
                           || (curPage == 2)      // Add Item
                           || (curPage == 3);     // Edit Item
        if (scanPage) {
        auto* k = static_cast<QKeyEvent*>(event);
        const int key = k->key();
        const Qt::KeyboardModifiers mods = k->modifiers();
        const bool alt   = (mods & Qt::AltModifier) != 0;
        const bool ctrl  = (mods & Qt::ControlModifier) != 0;
        const bool meta  = (mods & Qt::MetaModifier) != 0;
        const bool mod   = alt || ctrl || meta;

        // Enter/Return terminator of a scan burst.
        if ((key == Qt::Key_Return || key == Qt::Key_Enter) && !mod) {
            if (m_scannerInBurst && !m_scannerBuffer.isEmpty()
                && looksLikeBarcode(m_scannerBuffer)) {
                const QString code = m_scannerBuffer;
                resetScannerBurst();
                handleScannedCode(code);
                return true;        // consumed: never triggers normal Enter
            }
            // Not a scan burst (or a burst that contains spaces, i.e. a
            // human-typed phrase): drop the pending candidate and keep the
            // normal Enter behavior (sell highlighted item, submit form).
            if (m_scannerInBurst) resetScannerBurst();
            return QMainWindow::eventFilter(watched, event);
        }

        // Ignore pure modifier presses (Shift/Ctrl/Alt alone).
        if (key >= Qt::Key_Shift && key <= Qt::Key_AltGr) {
            return QMainWindow::eventFilter(watched, event);
        }

        if (!mod) {
            const QString text = k->text();
            if (text.size() == 1 && text.at(0).isPrint()) {
                const qint64 nowNs = m_scannerKeyTimer.nsecsElapsed();
                const qint64 gapNs = (m_scannerLastNs > 0) ? (nowNs - m_scannerLastNs)
                                                           : Q_INT64_C(-1);
                m_scannerGapNs = gapNs;
                m_scannerLastNs = nowNs;

                constexpr qint64 kScannerBurstNs = Q_INT64_C(80) * Q_INT64_C(1000) * Q_INT64_C(1000); // 80 ms
                if (m_scannerInBurst) {
                    if (gapNs >= 0 && gapNs <= kScannerBurstNs) {
                        if (m_scannerBuffer.size() < 64) m_scannerBuffer += text;
                    } else {
                        // Human pause ended the burst. Every character has
                        // already reached the focused widget, so just catch
                        // the card grid up with what the user typed.
                        resetScannerBurst();
                        applySellSearchFilter();
                    }
                } else if (gapNs >= 0 && gapNs <= kScannerBurstNs && !m_scannerBuffer.isEmpty()) {
                    // 2nd char right after the 1st (which was not consumed)
                    // confirms a burst → the buffered code is complete.
                    m_scannerInBurst = true;
                    if (m_scannerBuffer.size() < 64) m_scannerBuffer += text;
                } else {
                    // Candidate first character of a scan. Remember it but
                    // do NOT consume — a lone fast key must never disappear.
                    m_scannerBuffer = text;
                }
            }
        }
        // Printable scan characters always reach the focused widget: they
        // are never part of the "broken keyboard" path.
    }
    }

    if (event->type() == QEvent::KeyPress
        && watched != this
        && m_keybindsEnabled
        && m_isLoggedIn
        && !QApplication::activeModalWidget()   // no dialogs on top
        && ui->stackedWidget
        && ui->stackedWidget->currentIndex() == 6) {
        auto* k = static_cast<QKeyEvent*>(event);
        const int key = k->key();
        const Qt::KeyboardModifiers mods = k->modifiers();
        const bool altGr = (mods & Qt::ControlModifier) != 0;
        if (key != Qt::Key_Alt
            && (mods & Qt::AltModifier)
            && !altGr
            && !(mods & Qt::KeypadModifier)) {
            QList<int> reserved = menuAcceleratorKeys(this);
            if (key == Qt::Key_X) {
                // Alt+X = cancel the last sale. X is never a sell key
                // (see sellKeyIndex), so no product can be sold via Alt+X.
                on_btnUndoSale_sell_clicked();
                return true;   // consumed: never reaches the focused widget
            }
            int idx = sellKeyIndex(key, reserved);
            if (idx >= 0 && idx < m_sellCards.size()) {
                sellProductAtIndex(idx);
                return true;   // consumed: never reaches the focused widget
            }
        }
    }
    return QMainWindow::eventFilter(watched, event);
}

// Alt+digits/letters sell directly; arrows move the highlight; Enter
// sells the highlighted item. Only active on the sell page when keybinds
// are on. The Alt+key combos are handled from the application-level event
// filter (below) so they also work while the search box has focus.
void MainWindow::keyPressEvent(QKeyEvent *event)
{
    bool onSellPage = ui->stackedWidget
        && ui->stackedWidget->currentIndex() == 6
        && ui->txtSearch_sell_page
        && !ui->txtSearch_sell_page->hasFocus();

    if (m_keybindsEnabled && onSellPage && m_isLoggedIn) {
        int key = event->key();
        bool alt = (event->modifiers() & Qt::AltModifier) != 0;
        bool altGr = (event->modifiers() & Qt::ControlModifier) != 0;

        if (alt && !altGr && key != Qt::Key_Alt) {
            // Which Alt+letter combos are taken by the menu bar? Those
            // letters are dropped from the sell mapping ("functional/used
            // ones"), computed from the CURRENT (possibly translated)
            // top-level menu titles.
            QList<int> reserved = menuAcceleratorKeys(this);

            if (key == Qt::Key_X) {
                // Alt+X = cancel the last sale. X is never a sell key
                // (see sellKeyIndex), so no product can be sold via Alt+X.
                on_btnUndoSale_sell_clicked();
                event->accept();
                return;
            }
            int idx = sellKeyIndex(key, reserved);
            if (idx >= 0 && idx < m_sellCards.size()) {
                sellProductAtIndex(idx);
                event->accept();
                return;
            }
        }

        if (key == Qt::Key_Up || key == Qt::Key_Down
            || key == Qt::Key_Left || key == Qt::Key_Right) {
            if (!alt) {
                int width = ui->scrollAreaItems->viewport()->width();
                int cols = qMax(1, width / 260);
                int idx = m_sellHighlightIndex;
                if (idx < 0) idx = 0;
                else if (key == Qt::Key_Right && idx + 1 < m_sellCards.size()) idx += 1;
                else if (key == Qt::Key_Left && idx > 0) idx -= 1;
                else if (key == Qt::Key_Down && idx + cols < m_sellCards.size()) idx += cols;
                else if (key == Qt::Key_Up && idx - cols >= 0) idx -= cols;
                highlightSellCard(idx);
                event->accept();
                return;
            }
        }
        if (key == Qt::Key_Return || key == Qt::Key_Enter) {
            if (!alt && m_sellHighlightIndex >= 0 && m_sellHighlightIndex < m_sellCards.size()) {
                sellProductAtIndex(m_sellHighlightIndex);
                event->accept();
                return;
            }
        }
    }

    QMainWindow::keyPressEvent(event);
}

// ═══════════════════════════════════════════════════════════════════
// Categories
// ═══════════════════════════════════════════════════════════════════

void MainWindow::on_actionManage_Categories_triggered()
{
    if (!checkRoleRequired(BusinessLogic::RequiredRole::Admin)) return;
    goToPage(7); // Categories page
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
        QMessageBox::information(this, Tr::trS("Category"), Tr::trS("Category added."));
        m_worklog.logEntry(WorklogEntry::ActionType::Add, WorklogEntry::EntityType::Category,
                          id.toStdString(), "Added category: " + name.toStdString());
        on_actionManage_Categories_triggered();
    } else {
        QMessageBox::warning(this, Tr::trS("Category"), translatedBusinessMessage(result.errorMessage));
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
        QMessageBox::information(this, Tr::trS("Category"), Tr::trS("Category updated."));
        m_worklog.logEntry(WorklogEntry::ActionType::Edit, WorklogEntry::EntityType::Category,
                          dto.id, "Edited category: " + dto.name);
        on_actionManage_Categories_triggered();
    } else {
        QMessageBox::warning(this, Tr::trS("Category"), translatedBusinessMessage(result.errorMessage));
    }
}

void MainWindow::on_btnRemove_category_clicked()
{
    if (!checkRoleRequired(BusinessLogic::RequiredRole::Admin)) return;
    QString id = ui->txtId_category->text().trimmed();
    if (QMessageBox::question(this, Tr::trS("Confirm"), Tr::trS("Remove this category?")) == QMessageBox::Yes) {
        if (m_db.removeCategory(id.toStdString())) {
            QMessageBox::information(this, Tr::trS("Category"), Tr::trS("Category removed."));
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
    if (!checkRoleRequired(BusinessLogic::RequiredRole::Admin)) return;
    goToPage(8); // Shelves page
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
        QMessageBox::information(this, Tr::trS("Shelf"), Tr::trS("Shelf added."));
        m_worklog.logEntry(WorklogEntry::ActionType::Add, WorklogEntry::EntityType::Shelf,
                          id.toStdString(), "Added shelf: " + name.toStdString());
        on_actionManage_Shelves_triggered();
    } else {
        QMessageBox::warning(this, Tr::trS("Shelf"), translatedBusinessMessage(result.errorMessage));
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
        QMessageBox::information(this, Tr::trS("Shelf"), Tr::trS("Shelf updated."));
        m_worklog.logEntry(WorklogEntry::ActionType::Edit, WorklogEntry::EntityType::Shelf,
                          dto.id, "Edited shelf: " + dto.name);
        on_actionManage_Shelves_triggered();
    } else {
        QMessageBox::warning(this, Tr::trS("Shelf"), translatedBusinessMessage(result.errorMessage));
    }
}

void MainWindow::on_btnRemove_shelf_clicked()
{
    if (!checkRoleRequired(BusinessLogic::RequiredRole::Admin)) return;
    QString id = ui->txtId_shelf->text().trimmed();
    if (QMessageBox::question(this, Tr::trS("Confirm"), Tr::trS("Remove this shelf?")) == QMessageBox::Yes) {
        if (m_db.removeShelf(id.toStdString())) {
            QMessageBox::information(this, Tr::trS("Shelf"), Tr::trS("Shelf removed."));
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
    if (!checkRoleRequired(BusinessLogic::RequiredRole::Admin)) return;
    goToPage(9); // Sales History page
    on_btnRefresh_sales_clicked();
}

void MainWindow::populateSalesList(const std::vector<Domain::Sale>& sales)
{
    if (!ui->lstSearch_sales) return;

    auto items = m_db.getAllItems();
    std::unordered_map<std::string, std::string> names;
    for (const auto& it : items) names[it.id] = it.name;

    bool simple = ui->chkSimpleView_sales && ui->chkSimpleView_sales->isChecked();
    ui->lstSearch_sales->clear();
    for (const auto& s : sales) {
        QString text = simple ? saleListTextSimple(s, names) : saleListText(s, names);
        QListWidgetItem* lwi = new QListWidgetItem(text);
        lwi->setData(Qt::UserRole, QString::fromStdString(s.id));
        ui->lstSearch_sales->addItem(lwi);
    }
}

void MainWindow::on_btnRefresh_sales_clicked()
{
    auto sales = m_db.getAllSales();
    populateSalesList(sales);
    double totalRevenue = 0.0;
    for (const auto& s : sales) totalRevenue += s.totalAmount;
    if (ui->lblSalesTotal) {
        ui->lblSalesTotal->setText(Tr::trS("Total Revenue: ") + fmtMoney(totalRevenue));
    }
}

void MainWindow::on_btnSearch_sales_clicked()
{
    QString term = ui->txtSearch_sales->text().trimmed();
    auto sales = m_db.searchSales(term.toStdString(), "");
    populateSalesList(sales);
    double totalRevenue = 0.0;
    for (const auto& s : sales) totalRevenue += s.totalAmount;
    if (ui->lblSalesTotal) {
        ui->lblSalesTotal->setText(Tr::trS("Search Total: ") + fmtMoney(totalRevenue));
    }
}

void MainWindow::on_chkSimpleView_sales_toggled(bool)
{
    // Re-apply the simple/detailed format to whatever is currently shown,
    // keeping an active search filter if one is entered.
    if (ui->txtSearch_sales && !ui->txtSearch_sales->text().trimmed().isEmpty()) {
        on_btnSearch_sales_clicked();
    } else {
        on_btnRefresh_sales_clicked();
    }
}

void MainWindow::on_btnExport_sales_clicked()
{
    QString fileName = QFileDialog::getSaveFileName(this, Tr::trS("Export Sales"), "sales_export.csv", Tr::trS("CSV Files (*.csv)"));
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
    QMessageBox::information(this, Tr::trS("Export"), Tr::trS("Sales exported to %1").arg(fileName));
}

// ═══════════════════════════════════════════════════════════════════
// Reports
// ═══════════════════════════════════════════════════════════════════

void MainWindow::on_actionMake_Report_triggered()
{
    if (!checkRoleRequired(BusinessLogic::RequiredRole::Admin)) return;
    goToPage(10); // Report page
}

void MainWindow::on_btnGenerateReport_clicked()
{
    if (!checkRoleRequired(BusinessLogic::RequiredRole::Admin)) return;

    // All aggregation and report formatting live in the GUI-independent
    // Stats engine (statistics.h); the data layer also persists a daily
    // snapshot of the same numbers, so no statistics depend on this
    // window being open.
    auto sales = m_db.getAllSales();
    auto items = m_db.getAllItems();
    auto users = m_db.getAllUsers();
    std::unordered_map<std::string, std::string> itemNames;
    for (const auto& it : items) itemNames[it.id] = it.name;
    std::unordered_map<std::string, std::string> userNames;
    for (const auto& u : users) userNames[u.id] = u.username;

    auto snap = Stats::compute(sales, itemNames, userNames);

    if (ui->txtReport) {
        ui->txtReport->setText(Stats::buildReportText(snap, items));
    }

    // ── Statistics charts ───────────────────────────────────────
    // The charts live in a vertical splitter so each chart can be resized
    // individually; the marker property lets the PDF exporter (and only
    // it) pick the chart widgets out of the splitter.
    if (ui->splitReportCharts) {
        QSplitter* split = ui->splitReportCharts;

        // Clear any previously generated charts.
        while (split->count() > 0) {
            QWidget* old = split->widget(0);
            if (old) old->setParent(nullptr);   // detaches it from the splitter
            if (old) old->deleteLater();
        }

        // Pie: which items were sold the most (top 8 + "Other").
        auto* pie = new PieChartWidget;
        pie->setTitle(Tr::trS("Top Selling Items"));
        QVector<QPair<QString, double>> pieData;
        if (!snap.qtyByItem.empty()) {
            double other = 0.0;
            int shown = 0;
            for (const auto& p : snap.items) {
                if (++shown <= 8) {
                    pieData << QPair<QString, double>(p.name, double(p.qty));
                } else {
                    other += p.qty;
                }
            }
            if (other > 0.0) pieData << QPair<QString, double>(Tr::trS("Other"), other);
        }
        pie->setData(pieData);
        pie->setProperty("reportChart", true);
        split->addWidget(pie);

        // Bar: shop activity by hour of day (local time).
        auto* bar = new BarChartWidget;
        bar->setTitle(Tr::trS("Shop activity by hour"));
        QVector<double> barVals;
        QStringList barLabels;
        for (int h = 0; h < 24; ++h) {
            barVals << double(snap.hourlyActivity[h]);
            if (h % 4 == 0) barLabels << QString("%1:00").arg(h);
            else            barLabels << QString();
        }
        bar->setData(barVals, barLabels, 4);
        bar->setProperty("reportChart", true);
        split->addWidget(bar);

        // Sensible starting split: ~40% pie / 60% bar.
        split->setSizes({320, 420});
    }
}

void MainWindow::on_btnExportReport_clicked()
{
    if (ui->txtReport) {
        QString fileName = QFileDialog::getSaveFileName(this, Tr::trS("Export Report"), "report.txt", Tr::trS("Text Files (*.txt)"));
        if (!fileName.isEmpty()) {
            QFile file(fileName);
            if (file.open(QIODevice::WriteOnly | QIODevice::Text)) {
                QTextStream out(&file);
                out << ui->txtReport->toPlainText();
                file.close();
                QMessageBox::information(this, Tr::trS("Export"), Tr::trS("Report saved."));
            }
        }
    }
}

// Export the generated report (text + charts) as a PDF. The actual
// writing lives in pdf_export.h (QPdfWriter), so no extra Qt module is
// needed in the static builds. The suggested file name carries a
// timestamp so consecutive exports never silently overwrite each other.
void MainWindow::on_btnExportPdfReport_clicked()
{
    if (!ui->txtReport || ui->txtReport->toPlainText().isEmpty()) {
        QMessageBox::information(this, Tr::trS("Export PDF"),
                                 Tr::trS("Generate the report first."));
        return;
    }

    QString stamp = QDateTime::currentDateTime().toString("yyyy-MM-dd_hh-mm-ss");
    QString defaultName = QString("QMark_Report_%1.pdf").arg(stamp);
    QString fileName = QFileDialog::getSaveFileName(
        this, Tr::trS("Export PDF"), defaultName, Tr::trS("PDF Files") + " (*.pdf)");
    if (fileName.isEmpty()) return;
    if (!fileName.endsWith(".pdf", Qt::CaseInsensitive)) fileName += ".pdf";

    QString err;
    if (exportReportToPdf(fileName, ui->txtReport->toPlainText(),
                          ui->splitReportCharts, &err)) {
        QMessageBox::information(this, Tr::trS("Export PDF"), Tr::trS("PDF report saved."));
    } else {
        QMessageBox::warning(this, Tr::trS("Export PDF"),
                             Tr::trS("Could not save the PDF report.") + " (" + Tr::trS(err) + ")");
    }
}

void MainWindow::on_btnExitReport_clicked()
{
    goToPage(1); // dashboard
    refreshDashboard();
}

// ═══════════════════════════════════════════════════════════════════
// Database Selection
// ═══════════════════════════════════════════════════════════════════

void MainWindow::on_actionDatabase_Selection_triggered()
{
    if (!checkRoleRequired(BusinessLogic::RequiredRole::SuperAdmin)) return;
    goToPage(11); // DB Selection page
}

void MainWindow::on_actionCreate_Database_triggered()
{
    on_actionDatabase_Selection_triggered();
}

void MainWindow::on_btnBrowseItemsDb_clicked()
{
    QString file = QFileDialog::getSaveFileName(this, Tr::trS("Select Items Database"), "items.db", Tr::trS("SQLite DB (*.db)"));
    if (!file.isEmpty() && ui->txtItemsDbPath) {
        ui->txtItemsDbPath->setText(file);
    }
}

void MainWindow::on_btnBrowseUsersDb_clicked()
{
    QString file = QFileDialog::getSaveFileName(this, Tr::trS("Select Users Database"), "users.db", Tr::trS("SQLite DB (*.db)"));
    if (!file.isEmpty() && ui->txtUsersDbPath) {
        ui->txtUsersDbPath->setText(file);
    }
}

void MainWindow::on_btnLoadDbConfig_clicked()
{
    QString itemsDb = ui->txtItemsDbPath ? ui->txtItemsDbPath->text() : "";
    QString usersDb = ui->txtUsersDbPath ? ui->txtUsersDbPath->text() : "";

    if (itemsDb.isEmpty() || usersDb.isEmpty()) {
        QMessageBox::warning(this, Tr::trS("Database"), Tr::trS("Please select both database files."));
        return;
    }

    BusinessLogic::shutdownDatabases(m_db);
    if (BusinessLogic::initializeDatabases(m_db, itemsDb.toStdString(), usersDb.toStdString())) {
        QMessageBox::information(this, Tr::trS("Database"), Tr::trS("Connected to databases successfully."));
        LOG_INFO("Databases loaded: " + itemsDb + ", " + usersDb);
    } else {
        QMessageBox::warning(this, Tr::trS("Database"), Tr::trS("Failed to connect to databases."));
    }
}

void MainWindow::on_btnSaveAsDefault_clicked()
{
    QSettings settings("QMark", "SchoolShop");
    settings.setValue("db/itemsPath", ui->txtItemsDbPath ? ui->txtItemsDbPath->text() : "");
    settings.setValue("db/usersPath", ui->txtUsersDbPath ? ui->txtUsersDbPath->text() : "");
    QMessageBox::information(this, Tr::trS("Settings"), Tr::trS("Default database configuration saved."));
}

void MainWindow::on_btnTestConnection_clicked()
{
    if (m_db.isConnected()) {
        QMessageBox::information(this, Tr::trS("Test"), Tr::trS("Database connection is active."));
    } else {
        QMessageBox::warning(this, Tr::trS("Test"), Tr::trS("No database connection."));
    }
}

void MainWindow::on_btnCreateNewDb_clicked()
{
    QString itemsDb = ui->txtItemsDbPath ? ui->txtItemsDbPath->text() : "items.db";
    QString usersDb = ui->txtUsersDbPath ? ui->txtUsersDbPath->text() : "users.db";

    BusinessLogic::shutdownDatabases(m_db);
    if (BusinessLogic::initializeDatabases(m_db, itemsDb.toStdString(), usersDb.toStdString())) {
        QMessageBox::information(this, Tr::trS("Create DB"), Tr::trS("New databases created and connected."));
    } else {
        QMessageBox::warning(this, Tr::trS("Create DB"), Tr::trS("Failed to create databases."));
    }
}

void MainWindow::on_chkTelemetry_toggled(bool checked)
{
    QSettings settings("QMark", "SchoolShop");
    settings.setValue("telemetry/enabled", checked);
    AppLogger::instance().setTelemetryEnabled(checked);
    if (checked) {
        // Telemetry lives in the telemetry/ folder next to the app:
        // a human-readable .log (all LOG_* events) plus a SQLite .db.
        QString telDir = QDir::currentPath() + "/telemetry";
        QDir().mkpath(telDir);
        QString ts = QDateTime::currentDateTime().toString("yyyyMMdd_hhmmss");
        QString logPath = telDir + "/telemetry_" + ts + ".log";
        QString dbPath  = telDir + "/telemetry_" + ts + ".db";
        AppLogger::instance().setLogFile(logPath);
        telemetry().open(dbPath);
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
    goToPage(12); // Accounts page

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
    QString newRole = QInputDialog::getItem(this, Tr::trS("Change Role"), Tr::trS("Select new role:"), roles, 0, false, &ok);
    if (ok && !newRole.isEmpty()) {
        // Find user and update
        auto users = m_db.getAllUsers();
        for (auto& u : users) {
            if (u.id == userId.toStdString()) {
                if (newRole == "SuperAdmin") u.role = Domain::User::Role::SuperAdmin;
                else if (newRole == "Admin") u.role = Domain::User::Role::Admin;
                else u.role = Domain::User::Role::UserRole;
                m_db.updateUser(u);
                QMessageBox::information(this, Tr::trS("Account"), Tr::trS("Role changed."));
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
    QString newPwd = QInputDialog::getText(this, Tr::trS("Change Password"), Tr::trS("New password:"), QLineEdit::Password, "", &ok);
    if (ok && !newPwd.isEmpty()) {
        QString userId = selected->data(Qt::UserRole).toString();
        auto users = m_db.getAllUsers();
        for (auto& u : users) {
            if (u.id == userId.toStdString()) {
                // Never store plaintext passwords — hash before saving
                std::string hashedPw = hash_string(newPwd.toStdString());
                if (hashedPw.empty()) {
                    QMessageBox::warning(this, Tr::trS("Change Password"), Tr::trS("Failed to hash password."));
                    return;
                }
                u.passwordHash = hashedPw;
                m_db.updateUser(u);
                QMessageBox::information(this, Tr::trS("Account"), Tr::trS("Password changed."));
                break;
            }
        }
    }
}

void MainWindow::on_btnDeleteAccount_clicked()
{
    if (!checkRoleRequired(BusinessLogic::RequiredRole::SuperAdmin)) return;
    // Users cannot be deleted in current implementation — just a placeholder
    QMessageBox::information(this, Tr::trS("Account"), Tr::trS("Account deletion not yet implemented."));
}

// ═══════════════════════════════════════════════════════════════════
// Preferences
// ═══════════════════════════════════════════════════════════════════

void MainWindow::on_actionPreferences_triggered()
{
    if (!checkLoginRequired()) return;   // all roles (keybinds toggle)
    goToPage(13); // Preferences page

    // Currency switching is SuperAdmin-only
    bool canChangeCurrency = getCurrentUserRole() == Domain::User::Role::SuperAdmin;
    if (ui->cboCurrency_pref) ui->cboCurrency_pref->setEnabled(canChangeCurrency);
    if (ui->btnConvertPrices_pref) ui->btnConvertPrices_pref->setEnabled(canChangeCurrency);
    if (ui->lblCurrencyHint_pref) {
        ui->lblCurrencyHint_pref->setText(canChangeCurrency
            ? Tr::trS("Switching currency does not convert prices.")
            : Tr::trS("Only SuperAdmin can change the currency."));
    }

    // Sync the combo boxes / checkboxes with the saved settings
    QSettings settings("QMark", "SchoolShop");
    if (ui->cboLanguage_pref) {
        QString lang = settings.value("settings/language", "pl").toString();
        ui->cboLanguage_pref->setCurrentIndex(lang == "pl" ? 1 : 0);
    }
    if (ui->cboCurrency_pref) {
        QString cur = settings.value("settings/currency", "PLN").toString();
        ui->cboCurrency_pref->setCurrentIndex(cur == "USD" ? 1 : 0);
    }
    if (ui->chkKeybinds_pref) {
        ui->chkKeybinds_pref->setChecked(m_keybindsEnabled);
    }
}

void MainWindow::on_btnSavePreferences_clicked()
{
    QSettings settings("QMark", "SchoolShop");
    settings.setValue("worklog/enabled", ui->chkWorklog->isChecked());
    settings.setValue("telemetry/enabled", ui->chkTelemetry->isChecked());
    settings.setValue("settings/sellKeybinds", ui->chkKeybinds_pref->isChecked());

    // Language (all roles)
    QString lang = (ui->cboLanguage_pref && ui->cboLanguage_pref->currentIndex() == 1) ? "pl" : "en";
    settings.setValue("settings/language", lang);

    // Currency (SuperAdmin only)
    QString currency = (ui->cboCurrency_pref && ui->cboCurrency_pref->currentIndex() == 1) ? "USD" : "PLN";
    if (getCurrentUserRole() == Domain::User::Role::SuperAdmin) {
        settings.setValue("settings/currency", currency);
    } else {
        currency = settings.value("settings/currency", "PLN").toString();
    }
    Domain::setCurrencySymbol(currency == "USD" ? "$" : "zł");

    m_keybindsEnabled = ui->chkKeybinds_pref->isChecked();
    if (ui->chkKeybinds_sell) {
        ui->chkKeybinds_sell->blockSignals(true);
        ui->chkKeybinds_sell->setChecked(m_keybindsEnabled);
        ui->chkKeybinds_sell->blockSignals(false);
    }

    applyLanguageToUi();
    refreshSellPage();
    refreshDashboard();

    QMessageBox::information(this, Tr::trS("Preferences"), Tr::trS("Preferences saved."));
}

void MainWindow::on_btnResetPreferences_clicked()
{
    QSettings settings("QMark", "SchoolShop");

    // Keep the database paths, reset everything else
    QString itemsPath = settings.value("db/itemsPath").toString();
    QString usersPath = settings.value("db/usersPath").toString();
    settings.clear();
    if (!itemsPath.isEmpty()) settings.setValue("db/itemsPath", itemsPath);
    if (!usersPath.isEmpty()) settings.setValue("db/usersPath", usersPath);

    // Restore defaults in the UI
    Domain::setCurrencySymbol("zł");
    m_keybindsEnabled = true;
    ui->chkWorklog->setChecked(false);
    ui->chkTelemetry->setChecked(false);
    if (ui->chkKeybinds_pref) ui->chkKeybinds_pref->setChecked(true);
    if (ui->chkKeybinds_sell) {
        ui->chkKeybinds_sell->blockSignals(true);
        ui->chkKeybinds_sell->setChecked(true);
        ui->chkKeybinds_sell->blockSignals(false);
    }
    if (ui->cboLanguage_pref) ui->cboLanguage_pref->setCurrentIndex(1); // Polski
    if (ui->cboCurrency_pref) ui->cboCurrency_pref->setCurrentIndex(0); // PLN

    applyLanguageToUi();
    refreshSellPage();
    refreshDashboard();

    QMessageBox::information(this, Tr::trS("Preferences"), Tr::trS("Preferences reset to defaults."));
}

// Explicit price conversion (SuperAdmin only). Switching the currency in
// Preferences does NOT convert stored prices; this button does, using a
// user-provided factor (e.g. 1 PLN = 0.25 USD → factor 0.25).
void MainWindow::on_btnConvertPrices_pref_clicked()
{
    if (!checkRoleRequired(BusinessLogic::RequiredRole::SuperAdmin)) return;

    bool ok = false;
    double rate = QInputDialog::getDouble(this, Tr::trS("Convert All Prices..."),
        Tr::trS("Enter conversion factor (new price = old price × factor):"),
        1.0, 0.0001, 1000000.0, 4, &ok);
    if (!ok || rate <= 0.0) return;

    auto items = m_db.getAllItems();
    for (auto& it : items) {
        it.price = it.price * rate;
        it.updatedAt = Domain::now();
        m_db.updateItem(it);
    }

    LOG_INFO("Prices converted with factor " + QString::number(rate));
    QMessageBox::information(this, Tr::trS("Convert All Prices..."),
        QString::number(items.size()) + " " + Tr::trS("item(s) updated."));
    refreshSellPage();
    refreshDashboard();
}

void MainWindow::on_chkWorklog_toggled(bool checked)
{
    m_worklog.setEnabled(checked);
    if (checked) {
        // Worklogs live in the worklogs/ folder next to the app.
        QString workDir = QDir::currentPath() + "/worklogs";
        QDir().mkpath(workDir);
        QString logPath = workDir + "/" + Worklog::generateSessionFileName();
        m_worklog.setLogFile(logPath);
        m_worklogFilePath = logPath;
    } else {
        m_worklogFilePath.clear();
    }
}

// ═══════════════════════════════════════════════════════════════════
// Worklog Stats
// ═══════════════════════════════════════════════════════════════════

void MainWindow::on_actionWorklogStats_triggered()
{
    if (!checkRoleRequired(BusinessLogic::RequiredRole::Admin)) return;
    goToPage(14); // Worklog page
    on_btnRefreshWorklog_clicked();
}

void MainWindow::on_btnRefreshWorklog_clicked()
{
    if (!ui->txtWorklog) return;
    QString username = m_currentUser.has_value()
        ? QString::fromStdString(m_currentUser->username) : QString();
    QString stats;
    stats += "═══════════════════════════════════════\n";
    stats += "       " + Tr::trS("WORKLOG SESSION STATISTICS") + "\n";
    stats += "═══════════════════════════════════════\n";
    stats += Tr::trS("User: ") + username + "\n";
    stats += Tr::trS("Session Start: ") + m_worklog.getSessionStart().toString("yyyy-MM-dd hh:mm:ss") + "\n\n";
    stats += Tr::trS("Items Added: ") + QString::number(m_worklog.getItemAddCount()) + "\n";
    stats += Tr::trS("Items Edited: ") + QString::number(m_worklog.getItemEditCount()) + "\n";
    stats += Tr::trS("Items Removed: ") + QString::number(m_worklog.getItemRemoveCount()) + "\n";
    stats += Tr::trS("Sales Recorded: ") + QString::number(m_worklog.getSaleCount()) + "\n";
    stats += Tr::trS("Categories Added: ") + QString::number(m_worklog.getCategoryAddCount()) + "\n";
    stats += Tr::trS("Categories Edited: ") + QString::number(m_worklog.getCategoryEditCount()) + "\n";
    stats += Tr::trS("Categories Removed: ") + QString::number(m_worklog.getCategoryRemoveCount()) + "\n";
    stats += Tr::trS("Shelves Added: ") + QString::number(m_worklog.getShelfAddCount()) + "\n";
    stats += Tr::trS("Shelves Edited: ") + QString::number(m_worklog.getShelfEditCount()) + "\n";
    stats += Tr::trS("Shelves Removed: ") + QString::number(m_worklog.getShelfRemoveCount()) + "\n";
    stats += Tr::trS("Users Added: ") + QString::number(m_worklog.getUserAddCount()) + "\n";
    stats += Tr::trS("Users Edited: ") + QString::number(m_worklog.getUserEditCount()) + "\n";
    stats += Tr::trS("Users Removed: ") + QString::number(m_worklog.getUserRemoveCount()) + "\n";
    ui->txtWorklog->setText(stats);
}

void MainWindow::on_btnExportWorklog_clicked()
{
    QString fileName = QFileDialog::getSaveFileName(this, Tr::trS("Export Worklog"), "worklog_export.txt", Tr::trS("Text Files (*.txt)"));
    if (fileName.isEmpty()) return;

    QFile file(fileName);
    if (!file.open(QIODevice::WriteOnly | QIODevice::Text)) return;

    QTextStream out(&file);
    out << ui->txtWorklog->toPlainText();
    file.close();
    QMessageBox::information(this, Tr::trS("Export"), Tr::trS("Worklog exported."));
}

// ═══════════════════════════════════════════════════════════════════
// Troubleshoot
// ═══════════════════════════════════════════════════════════════════

void MainWindow::on_actionTroubleshoot_triggered()
{
    if (!checkRoleRequired(BusinessLogic::RequiredRole::Admin)) return;
    goToPage(15); // Troubleshoot page
}

void MainWindow::on_btnTestDbConnection_clicked()
{
    if (m_db.isConnected()) {
        QMessageBox::information(this, Tr::trS("DB Test"), Tr::trS("Connected."));
    } else {
        QMessageBox::warning(this, Tr::trS("DB Test"), Tr::trS("Not connected."));
    }
}

void MainWindow::on_btnViewLogs_clicked()
{
    QString logDir = QDir::currentPath();
    QMessageBox::information(this, Tr::trS("Logs"),
        Tr::trS("Log files are located in:") + "\n" + logDir);
}

void MainWindow::on_btnExportDiagnostics_clicked()
{
    // Timestamped default name, e.g. diagnostics_20260918_153012.txt
    QString defaultName = "diagnostics_"
        + QDateTime::currentDateTime().toString("yyyyMMdd_hhmmss") + ".txt";
    QString fileName = QFileDialog::getSaveFileName(this, Tr::trS("Export Diagnostics"),
        defaultName, Tr::trS("Text Files") + " (*.txt)");
    if (fileName.isEmpty()) return;

    QFile file(fileName);
    if (!file.open(QIODevice::WriteOnly | QIODevice::Text)) return;

    // Diagnostics content follows the selected UI language.
    QTextStream out(&file);
    out << Tr::trS("=== QMark Diagnostics ===") << "\n";
    out << Tr::trS("Date: ") << QDateTime::currentDateTime().toString("yyyy-MM-dd hh:mm:ss") << "\n";
    out << Tr::trS("DB Connected: ")
        << (m_db.isConnected() ? Tr::trS("Yes") : Tr::trS("No")) << "\n";
    out << Tr::trS("Logged In: ")
        << (m_isLoggedIn ? Tr::trS("Yes") : Tr::trS("No")) << "\n";
    if (m_currentUser.has_value()) {
        out << Tr::trS("User: ") << QString::fromStdString(m_currentUser->username) << "\n";
        out << Tr::trS("Role: ") << QString::fromStdString(m_currentUser->roleName()) << "\n";
    }
    out << Tr::trS("Telemetry: ")
        << (AppLogger::instance().isTelemetryEnabled() ? Tr::trS("On") : Tr::trS("Off")) << "\n";
    out << Tr::trS("Worklog: ")
        << (m_worklog.isEnabled() ? Tr::trS("On") : Tr::trS("Off")) << "\n";
    out << Tr::trS("Currency: ") << QString::fromStdString(Domain::currencySymbol()) << "\n";
    out << Tr::trS("Language: ") << Tr::language() << "\n";
    file.close();
    QMessageBox::information(this, Tr::trS("Export Diagnostics"), Tr::trS("Diagnostics exported."));
}

// ═══════════════════════════════════════════════════════════════════
// Automation & Remote (scanner / e-mail / backups / auto-update /
// remote dashboard). All network + disk work runs on worker threads;
// GUI updates are marshalled back through QMetaObject::invokeMethod
// with a QPointer guard so a closing window can never crash us.
// ═══════════════════════════════════════════════════════════════════

// Show a transient status-bar message (and always log it).
void MainWindow::showStatus(const QString& msg, int ms)
{
    if (statusBar()) statusBar()->showMessage(msg, ms > 0 ? ms : 7000);
    AppLogger::instance().log("AUTO", msg);
}

void MainWindow::on_actionAutomation_triggered()
{
    if (!checkRoleRequired(BusinessLogic::RequiredRole::SuperAdmin)) return;
    applyAutomationPageSettings();
    if (m_automationPage) goToPage(ui->stackedWidget->indexOf(m_automationPage));
}

void MainWindow::onAutomationSaveClicked()
{
    saveAutomationPageSettings();
}

// ── Automation page builder ─────────────────────────────────────
QWidget* MainWindow::buildAutomationPage()
{
    QWidget* page = new QWidget();
    page->setObjectName("automationPage");
    QVBoxLayout* mainLayout = new QVBoxLayout(page);
    mainLayout->setSpacing(0);
    mainLayout->setContentsMargins(0, 0, 0, 0);

    // ── Top bar ──
    QFrame* topBar = new QFrame();
    topBar->setMaximumHeight(60);
    topBar->setStyleSheet("background: #0078d4;");
    QHBoxLayout* topLayout = new QHBoxLayout(topBar);
    topLayout->setContentsMargins(15, 0, 15, 0);

    QPushButton* btnBack = new QPushButton(Tr::trS("← Back"));
    btnBack->setStyleSheet(
        "QPushButton { color: white; background: transparent; border: none; "
        "font-size: 14px; font-weight: bold; padding: 5px 10px; }"
        "QPushButton:hover { background: rgba(255,255,255,0.15); border-radius: 4px; }");
    btnBack->setCursor(Qt::PointingHandCursor);
    btnBack->setMinimumHeight(36);
    topLayout->addWidget(btnBack);

    QLabel* title = new QLabel(Tr::trS("Automation & Remote"));
    title->setStyleSheet("color: white; font-size: 16px; font-weight: bold;");
    topLayout->addWidget(title);
    topLayout->addStretch();
    mainLayout->addWidget(topBar);

    connect(btnBack, &QPushButton::clicked, this, [this]() {
        goToPage(1); // dashboard
    });

    // ── Scrollable content ──
    QScrollArea* scroll = new QScrollArea();
    scroll->setWidgetResizable(true);
    scroll->setFrameShape(QFrame::NoFrame);
    QWidget* body = new QWidget();
    QVBoxLayout* bodyLayout = new QVBoxLayout(body);
    bodyLayout->setContentsMargins(15, 15, 15, 15);
    bodyLayout->setSpacing(14);

    auto section = [](const QString& titleText) {
        QGroupBox* g = new QGroupBox(titleText);
        g->setStyleSheet(
            "QGroupBox { border: 1px solid #ccc; border-radius: 10px; margin-top: 12px; padding-top: 8px; }"
            "QGroupBox::title { subcontrol-origin: margin; left: 12px; padding: 0 4px; font-weight: bold; }");
        return g;
    };
    auto hint = [](const QString& text) {
        QLabel* l = new QLabel(text);
        l->setWordWrap(true);
        l->setStyleSheet("color: #666; background: transparent; font-size: 12px;");
        return l;
    };

    // ── 1. Scanner mode ──
    {
        QGroupBox* g = section(Tr::trS("Scanner mode"));
        QVBoxLayout* v = new QVBoxLayout(g);
        m_autoUi.chkScanner = new QCheckBox(Tr::trS("Enable barcode scanner mode (keyboard input)"));
        v->addWidget(m_autoUi.chkScanner);
        v->addWidget(hint(Tr::trS("Scans are read from any USB/HID barcode scanner quickly and sold automatically (POS page).")));
        bodyLayout->addWidget(g);

        connect(m_autoUi.chkScanner, &QCheckBox::toggled, this, [this](bool on) {
            setScannerMode(on);
            showStatus(Tr::trS(on ? "Scanner mode enabled." : "Scanner mode disabled."), 3000);
        });
    }

    // ── 2. Remote dashboard ──
    {
        QGroupBox* g = section(Tr::trS("Remote dashboard"));
        QVBoxLayout* v = new QVBoxLayout(g);

        m_autoUi.chkRemote = new QCheckBox(Tr::trS("Enable remote dashboard for SuperAdmin"));
        v->addWidget(m_autoUi.chkRemote);
        v->addWidget(hint(Tr::trS("A read-only dashboard + JSON API on the local network for remote SuperAdmin access.")));

        QGridLayout* grid = new QGridLayout();
        grid->setHorizontalSpacing(8);
        grid->setVerticalSpacing(6);
        grid->addWidget(new QLabel(Tr::trS("Port")), 0, 0);
        m_autoUi.spinRemotePort = new QSpinBox();
        m_autoUi.spinRemotePort->setRange(1, 65535);
        m_autoUi.spinRemotePort->setValue(8080);
        grid->addWidget(m_autoUi.spinRemotePort, 0, 1);

        grid->addWidget(new QLabel(Tr::trS("Access token (long & random)")), 1, 0);
        m_autoUi.txtRemoteToken = new QLineEdit();
        m_autoUi.txtRemoteToken->setPlaceholderText(Tr::trS("Access token (long & random)"));
        grid->addWidget(m_autoUi.txtRemoteToken, 1, 1);

        QPushButton* btnToken = new QPushButton(Tr::trS("Generate token"));
        grid->addWidget(btnToken, 1, 2);
        v->addLayout(grid);

        m_autoUi.lblRemoteStatus = new QLabel();
        m_autoUi.lblRemoteStatus->setWordWrap(true);
        m_autoUi.lblRemoteStatus->setStyleSheet("color: #2e7d32; font-weight: bold; background: transparent;");
        v->addWidget(m_autoUi.lblRemoteStatus);
        v->addWidget(hint(Tr::trS("Security warning: the token is the only gate — keep it secret.")));
        bodyLayout->addWidget(g);

        connect(btnToken, &QPushButton::clicked, this, [this]() {
            QString token;
            for (int i = 0; i < 6; ++i)
                token += QString::number(QRandomGenerator::system()->generate(), 16);
            m_autoUi.txtRemoteToken->setText(token);
            AppSettings::settings().setValue(AppSettings::keyRemoteToken(), token);
            // If the dashboard is already up, restart it so the new token
            // takes effect immediately (the running server still holds the
            // old one until it is rebuilt).
            if (m_remoteServer) {
                stopRemoteServer();
                startRemoteServerIfEnabled();
            }
            showStatus(Tr::trS("Access token (long & random)") + " ✓", 3000);
        });
        connect(m_autoUi.chkRemote, &QCheckBox::toggled, this, [this](bool) {
            saveAutomationPageSettings();
        });
    }

    // ── 3. E-mail summary ──
    {
        QGroupBox* g = section(Tr::trS("E-mail summary"));
        QVBoxLayout* v = new QVBoxLayout(g);

        m_autoUi.chkMail = new QCheckBox(Tr::trS("Enable scheduled e-mail summaries"));
        v->addWidget(m_autoUi.chkMail);

        QGridLayout* grid = new QGridLayout();
        grid->setHorizontalSpacing(8);
        grid->setVerticalSpacing(6);
        grid->addWidget(new QLabel(Tr::trS("SMTP host")), 0, 0);
        m_autoUi.txtMailHost = new QLineEdit();
        grid->addWidget(m_autoUi.txtMailHost, 0, 1);
        grid->addWidget(new QLabel(Tr::trS("SMTP port")), 0, 2);
        m_autoUi.spinMailPort = new QSpinBox();
        m_autoUi.spinMailPort->setRange(1, 65535);
        m_autoUi.spinMailPort->setValue(587);
        grid->addWidget(m_autoUi.spinMailPort, 0, 3);

        grid->addWidget(new QLabel(Tr::trS("Security")), 1, 0);
        m_autoUi.cboMailSecurity = new QComboBox();
        m_autoUi.cboMailSecurity->addItems({ "STARTTLS (587)", "Implicit TLS (465)", "Plaintext (25)" });
        grid->addWidget(m_autoUi.cboMailSecurity, 1, 1);
        grid->addWidget(new QLabel(Tr::trS("Sender address")), 1, 2);
        m_autoUi.txtMailFrom = new QLineEdit();
        grid->addWidget(m_autoUi.txtMailFrom, 1, 3);

        grid->addWidget(new QLabel(Tr::trS("Username")), 2, 0);
        m_autoUi.txtMailUser = new QLineEdit();
        grid->addWidget(m_autoUi.txtMailUser, 2, 1);
        grid->addWidget(new QLabel(Tr::trS("Password")), 2, 2);
        m_autoUi.txtMailPass = new QLineEdit();
        m_autoUi.txtMailPass->setEchoMode(QLineEdit::Password);
        grid->addWidget(m_autoUi.txtMailPass, 2, 3);
        v->addLayout(grid);

        v->addWidget(new QLabel(Tr::trS("Recipients (one per line)")));
        m_autoUi.txtMailRecipients = new QPlainTextEdit();
        m_autoUi.txtMailRecipients->setMaximumHeight(70);
        m_autoUi.txtMailRecipients->setPlaceholderText(
            "admin@example.com\nmanager@example.com");
        v->addWidget(m_autoUi.txtMailRecipients);

        QGridLayout* sched = new QGridLayout();
        m_autoUi.chkMailDaily = new QCheckBox(Tr::trS("Send daily summary"));
        m_autoUi.timeMailDaily = new QTimeEdit();
        m_autoUi.timeMailDaily->setDisplayFormat("HH:mm");
        sched->addWidget(m_autoUi.chkMailDaily, 0, 0);
        sched->addWidget(m_autoUi.timeMailDaily, 0, 1);

        m_autoUi.chkMailMonthly = new QCheckBox(Tr::trS("Send monthly summary"));
        m_autoUi.spinMailMonthlyDay = new QSpinBox();
        m_autoUi.spinMailMonthlyDay->setRange(1, 28);
        m_autoUi.spinMailMonthlyDay->setValue(1);
        m_autoUi.timeMailMonthly = new QTimeEdit();
        m_autoUi.timeMailMonthly->setDisplayFormat("HH:mm");
        QHBoxLayout* monthlyRow = new QHBoxLayout();
        monthlyRow->addWidget(m_autoUi.chkMailMonthly);
        monthlyRow->addWidget(new QLabel(Tr::trS("Day of month")));
        monthlyRow->addWidget(m_autoUi.spinMailMonthlyDay);
        monthlyRow->addWidget(m_autoUi.timeMailMonthly);
        monthlyRow->addStretch();
        sched->addLayout(monthlyRow, 1, 0, 1, 2);
        v->addLayout(sched);

        m_autoUi.chkMailAttach = new QCheckBox(Tr::trS("Attach database backup to each summary"));
        v->addWidget(m_autoUi.chkMailAttach);

        m_autoUi.lblMailStatus = new QLabel();
        m_autoUi.lblMailStatus->setWordWrap(true);
        m_autoUi.lblMailStatus->setStyleSheet("color: #2e7d32; background: transparent;");
        v->addWidget(m_autoUi.lblMailStatus);

        QPushButton* btnTest = new QPushButton(Tr::trS("Send test e-mail"));
        v->addWidget(btnTest);
        bodyLayout->addWidget(g);

        connect(btnTest, &QPushButton::clicked, this, [this]() {
            saveAutomationPageSettings();
            sendSummaryEmailNow(false);
        });
    }

    // ── 4. Backups ──
    {
        QGroupBox* g = section(Tr::trS("Backups"));
        QVBoxLayout* v = new QVBoxLayout(g);

        QGridLayout* grid = new QGridLayout();
        grid->setHorizontalSpacing(8);
        grid->setVerticalSpacing(6);
        grid->addWidget(new QLabel(Tr::trS("Backup folder")), 0, 0);
        m_autoUi.txtBackupFolder = new QLineEdit();
        grid->addWidget(m_autoUi.txtBackupFolder, 0, 1);
        grid->addWidget(new QLabel(Tr::trS("Keep last N backups")), 0, 2);
        m_autoUi.spinBackupKeep = new QSpinBox();
        m_autoUi.spinBackupKeep->setRange(1, 100);
        m_autoUi.spinBackupKeep->setValue(10);
        grid->addWidget(m_autoUi.spinBackupKeep, 0, 3);
        v->addLayout(grid);

        QHBoxLayout* sched = new QHBoxLayout();
        m_autoUi.chkBackupSchedule = new QCheckBox(Tr::trS("Scheduled backup"));
        m_autoUi.timeBackupSchedule = new QTimeEdit();
        m_autoUi.timeBackupSchedule->setDisplayFormat("HH:mm");
        sched->addWidget(m_autoUi.chkBackupSchedule);
        sched->addWidget(new QLabel(Tr::trS("Time of day")));
        sched->addWidget(m_autoUi.timeBackupSchedule);
        sched->addStretch();
        v->addLayout(sched);

        m_autoUi.chkBackupOnlineMail = new QCheckBox(Tr::trS("Send a copy online via e-mail"));
        v->addWidget(m_autoUi.chkBackupOnlineMail);
        m_autoUi.chkBackupOnlineHttp = new QCheckBox(Tr::trS("Upload a copy to a custom server (HTTP PUT)"));
        v->addWidget(m_autoUi.chkBackupOnlineHttp);

        QGridLayout* httpGrid = new QGridLayout();
        httpGrid->setHorizontalSpacing(8);
        httpGrid->setVerticalSpacing(6);
        httpGrid->addWidget(new QLabel(Tr::trS("Server URL (or {filename} template)")), 0, 0);
        m_autoUi.txtBackupHttpUrl = new QLineEdit();
        httpGrid->addWidget(m_autoUi.txtBackupHttpUrl, 0, 1);
        httpGrid->addWidget(new QLabel(Tr::trS("Bearer token (optional)")), 1, 0);
        m_autoUi.txtBackupHttpToken = new QLineEdit();
        m_autoUi.txtBackupHttpToken->setEchoMode(QLineEdit::Password);
        httpGrid->addWidget(m_autoUi.txtBackupHttpToken, 1, 1);
        v->addLayout(httpGrid);

        m_autoUi.lblBackupStatus = new QLabel();
        m_autoUi.lblBackupStatus->setWordWrap(true);
        m_autoUi.lblBackupStatus->setStyleSheet("color: #2e7d32; background: transparent;");
        v->addWidget(m_autoUi.lblBackupStatus);

        QPushButton* btnBackup = new QPushButton(Tr::trS("Back up now"));
        v->addWidget(btnBackup);
        bodyLayout->addWidget(g);

        connect(btnBackup, &QPushButton::clicked, this, [this]() {
            saveAutomationPageSettings();
            createBackupNow();
        });
    }

    // ── 5. Auto-update ──
    {
        QGroupBox* g = section(Tr::trS("Auto-update"));
        QVBoxLayout* v = new QVBoxLayout(g);

        m_autoUi.chkUpdates = new QCheckBox(Tr::trS("Enable automatic update checks from GitHub"));
        v->addWidget(m_autoUi.chkUpdates);
        m_autoUi.lblUpdateStatus = new QLabel();
        m_autoUi.lblUpdateStatus->setWordWrap(true);
        m_autoUi.lblUpdateStatus->setStyleSheet("color: #2e7d32; background: transparent;");
        v->addWidget(m_autoUi.lblUpdateStatus);

        QPushButton* btnCheck = new QPushButton(Tr::trS("Check for updates now"));
        v->addWidget(btnCheck);
        bodyLayout->addWidget(g);

        connect(btnCheck, &QPushButton::clicked, this, [this]() {
            checkUpdatesNow();
        });

        QPushButton* btnRestore = new QPushButton(Tr::trS("Restore previous release"));
        v->addWidget(btnRestore);
        connect(btnRestore, &QPushButton::clicked, this, &MainWindow::restorePreviousRelease);
    }

    // ── Save button ──
    QPushButton* btnSave = new QPushButton(Tr::trS("Save Automation Settings"));
    btnSave->setObjectName("btnAutomation_pref");
    btnSave->setMinimumHeight(44);
    btnSave->setStyleSheet(
        "QPushButton { background-color: #0078d4; color: white; border: none; border-radius: 8px; "
        "font-size: 14px; font-weight: bold; padding: 8px 20px; }"
        "QPushButton:hover { background-color: #005a9e; }");
    btnSave->setCursor(Qt::PointingHandCursor);
    bodyLayout->addWidget(btnSave);
    connect(btnSave, &QPushButton::clicked, this, &MainWindow::onAutomationSaveClicked);

    scroll->setWidget(body);
    mainLayout->addWidget(scroll);

    // Register into the stack (page index is dynamic, like resupply).
    if (ui->stackedWidget) ui->stackedWidget->addWidget(page);
    applyAutomationPageSettings();
    return page;
}

// ── Automation settings: page → members → widget values ──────────
void MainWindow::applyAutomationPageSettings()
{
    // Populating the checkboxes emits toggled(), whose handlers persist the
    // (still half-filled) UI back to settings. Guard against that re-entrancy
    // so an existing configuration is never overwritten with defaults.
    m_applyingAutomationSettings = true;
    struct Guard {
        bool* flag;
        ~Guard() { *flag = false; }
    } guard{&m_applyingAutomationSettings};

    // Scanner
    if (m_autoUi.chkScanner) {
        m_autoUi.chkScanner->setChecked(AppSettings::scanEnabled());
        m_scannerEnabled = AppSettings::scanEnabled();
    }
    // Remote
    if (m_autoUi.chkRemote) {
        m_autoUi.chkRemote->setChecked(AppSettings::remoteEnabled());
        m_autoUi.spinRemotePort->setValue(AppSettings::remotePort());
        m_autoUi.txtRemoteToken->setText(AppSettings::remoteToken());
        if (m_autoUi.lblRemoteStatus) {
            if (m_remoteServer && m_remoteServer->isRunning())
                m_autoUi.lblRemoteStatus->setText(
                    Tr::trS("Running on port %1").arg(m_remoteServer->effectivePort()));
            else if (AppSettings::remoteEnabled() && !AppSettings::remoteToken().isEmpty())
                m_autoUi.lblRemoteStatus->setText(Tr::trS("Dashboard stopped."));
            else
                m_autoUi.lblRemoteStatus->setText(Tr::trS("Dashboard stopped."));
        }
    }
    // E-mail
    if (m_autoUi.chkMail) {
        m_autoUi.chkMail->setChecked(AppSettings::mailEnabled());
        m_autoUi.txtMailHost->setText(AppSettings::mailHost());
        m_autoUi.spinMailPort->setValue(AppSettings::mailPort());
        m_autoUi.cboMailSecurity->setCurrentIndex(qBound(0, AppSettings::mailSecurity(), 2));
        m_autoUi.txtMailUser->setText(AppSettings::mailUsername());
        m_autoUi.txtMailPass->setText(AppSettings::mailPassword());
        m_autoUi.txtMailFrom->setText(AppSettings::mailFrom());
        m_autoUi.txtMailRecipients->setPlainText(AppSettings::mailRecipients().join("\n"));
        m_autoUi.chkMailDaily->setChecked(AppSettings::mailDailyEnabled());
        m_autoUi.timeMailDaily->setTime(QTime::fromString(AppSettings::mailDailyTime(), "HH:mm"));
        m_autoUi.chkMailMonthly->setChecked(AppSettings::mailMonthlyEnabled());
        m_autoUi.spinMailMonthlyDay->setValue(AppSettings::mailMonthlyDay());
        m_autoUi.timeMailMonthly->setTime(QTime::fromString(AppSettings::mailMonthlyTime(), "HH:mm"));
        m_autoUi.chkMailAttach->setChecked(AppSettings::mailAttachBackup());
        if (m_autoUi.lblMailStatus) {
            const QString lastMail = !AppSettings::mailDailyLast().isEmpty()
                ? AppSettings::mailDailyLast() : AppSettings::mailMonthlyLast();
            m_autoUi.lblMailStatus->setText(lastMail.isEmpty()
                ? Tr::trS("Last e-mail: none yet")
                : Tr::trS("Last e-mail: %1").arg(lastMail));
        }
    }
    // Backups
    if (m_autoUi.txtBackupFolder) {
        m_autoUi.txtBackupFolder->setText(AppSettings::backupFolder());
        m_autoUi.spinBackupKeep->setValue(AppSettings::backupKeep());
        m_autoUi.chkBackupSchedule->setChecked(AppSettings::backupScheduleEnabled());
        m_autoUi.timeBackupSchedule->setTime(QTime::fromString(AppSettings::backupScheduleTime(), "HH:mm"));
        m_autoUi.chkBackupOnlineMail->setChecked(AppSettings::backupOnlineMail());
        m_autoUi.chkBackupOnlineHttp->setChecked(AppSettings::backupOnlineHttp());
        m_autoUi.txtBackupHttpUrl->setText(AppSettings::backupHttpUrl());
        m_autoUi.txtBackupHttpToken->setText(AppSettings::backupHttpToken());
        if (m_autoUi.lblBackupStatus) {
            const QString last = AppSettings::backupLastAt();
            m_autoUi.lblBackupStatus->setText(last.isEmpty()
                ? Tr::trS("Last backup: none yet")
                : Tr::trS("Last backup: %1").arg(last));
        }
    }
    // Auto-update
    if (m_autoUi.chkUpdates) {
        m_autoUi.chkUpdates->setChecked(AppSettings::updateEnabled());
        if (m_autoUi.lblUpdateStatus) {
            const QString seen = AppSettings::updateLastSeen();
            m_autoUi.lblUpdateStatus->setText(seen.isEmpty()
                ? Tr::trS("Update check: up to date.")
                : Tr::trS("Update available: %1").arg(seen));
        }
    }
}

// Persist the widget values, apply live state, restart the dashboard.
void MainWindow::saveAutomationPageSettings()
{
    // Ignore saves triggered while the page is still being populated.
    if (m_applyingAutomationSettings) return;

    // Scanner
    m_scannerEnabled = m_autoUi.chkScanner ? m_autoUi.chkScanner->isChecked()
                                           : AppSettings::scanEnabled();
    setScannerMode(m_scannerEnabled);
    // Remote
    if (m_autoUi.chkRemote) {
        AppSettings::settings().setValue(AppSettings::keyRemoteEnabled(), m_autoUi.chkRemote->isChecked());
        AppSettings::settings().setValue(AppSettings::keyRemotePort(), m_autoUi.spinRemotePort->value());
        AppSettings::settings().setValue(AppSettings::keyRemoteToken(),
                                         m_autoUi.txtRemoteToken->text().trimmed());
    }
    // E-mail
    if (m_autoUi.chkMail) {
        AppSettings::settings().setValue(AppSettings::keyMailEnabled(), m_autoUi.chkMail->isChecked());
        AppSettings::settings().setValue(AppSettings::keyMailHost(), m_autoUi.txtMailHost->text().trimmed());
        AppSettings::settings().setValue(AppSettings::keyMailPort(), m_autoUi.spinMailPort->value());
        AppSettings::settings().setValue(AppSettings::keyMailSecurity(), m_autoUi.cboMailSecurity->currentIndex());
        AppSettings::settings().setValue(AppSettings::keyMailUsername(), m_autoUi.txtMailUser->text().trimmed());
        AppSettings::settings().setValue(AppSettings::keyMailPassword(), m_autoUi.txtMailPass->text());
        AppSettings::settings().setValue(AppSettings::keyMailFrom(), m_autoUi.txtMailFrom->text().trimmed());
        QStringList recipients;
        for (const QString& line : m_autoUi.txtMailRecipients->toPlainText().split('\n'))
            if (!line.trimmed().isEmpty()) recipients << line.trimmed();
        AppSettings::settings().setValue(AppSettings::keyMailRecipients(), recipients);
        AppSettings::settings().setValue(AppSettings::keyMailDailyEnabled(), m_autoUi.chkMailDaily->isChecked());
        AppSettings::settings().setValue(AppSettings::keyMailDailyTime(),
                                         m_autoUi.timeMailDaily->time().toString("HH:mm"));
        AppSettings::settings().setValue(AppSettings::keyMailMonthlyEnabled(), m_autoUi.chkMailMonthly->isChecked());
        AppSettings::settings().setValue(AppSettings::keyMailMonthlyDay(), m_autoUi.spinMailMonthlyDay->value());
        AppSettings::settings().setValue(AppSettings::keyMailMonthlyTime(),
                                         m_autoUi.timeMailMonthly->time().toString("HH:mm"));
        AppSettings::settings().setValue(AppSettings::keyMailAttachBackup(), m_autoUi.chkMailAttach->isChecked());
    }
    // Backups
    if (m_autoUi.txtBackupFolder) {
        AppSettings::settings().setValue(AppSettings::keyBackupFolder(), m_autoUi.txtBackupFolder->text().trimmed());
        AppSettings::settings().setValue(AppSettings::keyBackupKeep(), m_autoUi.spinBackupKeep->value());
        AppSettings::settings().setValue(AppSettings::keyBackupScheduleEnabled(), m_autoUi.chkBackupSchedule->isChecked());
        AppSettings::settings().setValue(AppSettings::keyBackupScheduleTime(),
                                         m_autoUi.timeBackupSchedule->time().toString("HH:mm"));
        AppSettings::settings().setValue(AppSettings::keyBackupOnlineMail(), m_autoUi.chkBackupOnlineMail->isChecked());
        AppSettings::settings().setValue(AppSettings::keyBackupOnlineHttp(), m_autoUi.chkBackupOnlineHttp->isChecked());
        AppSettings::settings().setValue(AppSettings::keyBackupHttpUrl(), m_autoUi.txtBackupHttpUrl->text().trimmed());
        AppSettings::settings().setValue(AppSettings::keyBackupHttpToken(), m_autoUi.txtBackupHttpToken->text().trimmed());
    }
    // Auto-update
    if (m_autoUi.chkUpdates) {
        AppSettings::settings().setValue(AppSettings::keyUpdateEnabled(), m_autoUi.chkUpdates->isChecked());
    }

    startRemoteServerIfEnabled();
    showStatus(Tr::trS("Automation settings saved."), 3000);
}

// ── Remote dashboard server ─────────────────────────────────────
void MainWindow::startRemoteServerIfEnabled()
{
    const QString token = AppSettings::remoteToken().trimmed();
    // The token is the only gate on a network-exposed, plaintext service, so
    // refuse to run with a missing or trivially guessable one.
    if (!AppSettings::remoteEnabled() || token.size() < 16) {
        stopRemoteServer();
        if (m_autoUi.lblRemoteStatus) {
            m_autoUi.lblRemoteStatus->setText(
                AppSettings::remoteEnabled()
                    ? Tr::trS("Remote token must be at least %1 characters.").arg(16)
                    : Tr::trS("Dashboard stopped."));
        }
        return;
    }
    if (!m_remoteServer) {
        m_remoteServer = new RemoteServer(m_db, this);
        connect(m_remoteServer, &RemoteServer::logMessage, this, [this](const QString& line) {
            AppLogger::instance().log("REMOTE", line);
        });
    }
    if (m_remoteServer->isRunning()) {
        if (m_autoUi.lblRemoteStatus)
            m_autoUi.lblRemoteStatus->setText(
                Tr::trS("Running on port %1").arg(m_remoteServer->effectivePort()));
        return;
    }
    QString err;
    if (!m_remoteServer->start(quint16(AppSettings::remotePort()), token, &err)) {
        if (m_autoUi.lblRemoteStatus)
            m_autoUi.lblRemoteStatus->setText(Tr::trS("Dashboard error: %1").arg(Tr::trS(err)));
        showStatus(Tr::trS("Dashboard error: %1").arg(Tr::trS(err)), 8000);
        return;
    }
    const QString url = QString("http://%1:%2/?token=%3")
        .arg(QHostInfo::localHostName())
        .arg(m_remoteServer->effectivePort())
        .arg(token);
    if (m_autoUi.lblRemoteStatus) {
        m_autoUi.lblRemoteStatus->setText(
            Tr::trS("Running on port %1").arg(m_remoteServer->effectivePort())
            + "\n" + Tr::trS("Dashboard URL: ") + url);
    }
    showStatus(Tr::trS("Running on port %1").arg(m_remoteServer->effectivePort()), 5000);
}

void MainWindow::stopRemoteServer()
{
    if (m_remoteServer) {
        m_remoteServer->stop();
        delete m_remoteServer;
        m_remoteServer = nullptr;
    }
}

// ── Scheduler: daily/monthly e-mail + scheduled backups ─────────
void MainWindow::onSchedulerTick()
{
    const QString now = QTime::currentTime().toString("HH:mm");
    const QString today = QDate::currentDate().toString("yyyy-MM-dd");

    // Daily summary e-mail.
    if (AppSettings::mailEnabled() && AppSettings::mailDailyEnabled()
        && now == AppSettings::mailDailyTime()
        && AppSettings::mailDailyLast() != today) {
        AppSettings::settings().setValue(AppSettings::keyMailDailyLast(), today);
        sendSummaryEmailNow(false);
    }

    // Monthly summary e-mail (fires on the configured day of month).
    if (AppSettings::mailEnabled() && AppSettings::mailMonthlyEnabled()
        && QDate::currentDate().day() == AppSettings::mailMonthlyDay()
        && now == AppSettings::mailMonthlyTime()
        && AppSettings::mailMonthlyLast() != QDate::currentDate().toString("yyyy-MM")) {
        AppSettings::settings().setValue(AppSettings::keyMailMonthlyLast(),
                                         QDate::currentDate().toString("yyyy-MM"));
        sendSummaryEmailNow(true);
    }

    // Scheduled local backup.
    if (AppSettings::backupScheduleEnabled()
        && now == AppSettings::backupScheduleTime()
        && AppSettings::backupScheduleLast() != today) {
        AppSettings::settings().setValue(AppSettings::keyBackupScheduleLast(), today);
        createBackupNow();
    }
}

// ── E-mail summary (blocking SMTP on a worker thread) ───────────
void MainWindow::sendSummaryEmailNow(bool monthly)
{
    if (m_mailInProgress) {
        showStatus(Tr::trS("E-mail sending is already in progress."), 4000);
        return;
    }

    const QStringList recipients = AppSettings::mailRecipients();
    SmtpSettings st;
    st.host = AppSettings::mailHost();
    st.port = AppSettings::mailPort();
    st.security = AppSettings::mailSecurity();
    st.username = AppSettings::mailUsername();
    st.password = AppSettings::mailPassword();
    st.from = AppSettings::mailFrom();
    st.fromName = "QMark";

    if (st.host.isEmpty() || st.from.isEmpty() || recipients.isEmpty()) {
        showStatus(Tr::trS("E-mail settings are incomplete."), 6000);
        return;
    }

    SmtpMessage msg;
    msg.subject = "QMark — " + Tr::trS(monthly ? "MONTHLY SHOP SUMMARY" : "DAILY SHOP SUMMARY");
    msg.bodyPlain = Summary::buildText(monthly, m_db);
    msg.recipients = recipients;
    if (AppSettings::mailAttachBackup()) {
        msg.attachments.append({ Backup::archiveFileName(QDateTime::currentDateTime()),
                                 Backup::buildBundle() });
    }

    showStatus(Tr::trS("Sending e-mail…"), 4000);
    if (m_autoUi.lblMailStatus) m_autoUi.lblMailStatus->setText(Tr::trS("Sending e-mail…"));
    m_mailInProgress = true;

    QPointer<MainWindow> guard = this;
    std::thread([guard, st, msg, monthly]() {
        QString err;
        const bool ok = smtpSend(st, msg, 60000, &err);
        if (!guard) return;
        QMetaObject::invokeMethod(guard, [guard, ok, err, monthly]() {
            if (!guard) return;
            guard->m_mailInProgress = false;
            if (ok) {
                const QString s = monthly ? Tr::trS("Monthly summary sent.")
                                          : Tr::trS("Daily summary sent.");
                if (guard->m_autoUi.lblMailStatus) guard->m_autoUi.lblMailStatus->setText(s);
                guard->showStatus(s, 6000);
            } else {
                const QString s = Tr::trS("E-mail failed: %1").arg(Tr::trS(err));
                if (guard->m_autoUi.lblMailStatus) guard->m_autoUi.lblMailStatus->setText(s);
                guard->showStatus(s, 8000);
            }
        }, Qt::QueuedConnection);
    }).detach();
}

// ── Backups: local ZIP + optional online legs (worker thread) ──
void MainWindow::createBackupNow()
{
    if (m_backupInProgress) {
        showStatus(Tr::trS("Backup already in progress."), 4000);
        return;
    }
    m_backupInProgress = true;
    if (m_autoUi.lblBackupStatus) {
        m_autoUi.lblBackupStatus->setText(Tr::trS("Creating backup…"));
        m_autoUi.lblBackupStatus->setStyleSheet("color: #2e7d32; background: transparent;");
    }

    const QString folder = AppSettings::backupFolder();
    const int keep = AppSettings::backupKeep();
    const bool onlineHttp = AppSettings::backupOnlineHttp();
    const bool onlineMail = AppSettings::backupOnlineMail();
    const QString httpUrl = AppSettings::backupHttpUrl();
    const QString httpToken = AppSettings::backupHttpToken();
    const QStringList recipients = AppSettings::mailRecipients();

    SmtpSettings st;
    st.host = AppSettings::mailHost();
    st.port = AppSettings::mailPort();
    st.security = AppSettings::mailSecurity();
    st.username = AppSettings::mailUsername();
    st.password = AppSettings::mailPassword();
    st.from = AppSettings::mailFrom();
    st.fromName = "QMark";

    QPointer<MainWindow> guard = this;
    std::thread([guard, folder, keep, onlineHttp, onlineMail, httpUrl, httpToken,
                 recipients, st]() {
        QString err;
        const QString path = Backup::createLocal(folder, keep, &err);
        const QString stamp = QDateTime::currentDateTime().toString("yyyy-MM-dd hh:mm:ss");
        QString report;
        if (path.isEmpty()) {
            report = Tr::trS("Backup failed: %1").arg(Tr::trS(err));
        } else {
            report = Tr::trS("Backup created: %1").arg(path);
        }

        // Online legs (only when the local backup succeeded and enabled).
        if (!path.isEmpty()) {
            const QByteArray bundle = Backup::buildBundle();
            const QString fileName = Backup::archiveFileName(QDateTime::currentDateTime());
            if (onlineHttp) {
                QString uerr;
                if (!Backup::uploadHttp(httpUrl, httpToken, bundle, fileName, &uerr))
                    report += "\n" + Tr::trS("Online upload failed: %1").arg(Tr::trS(uerr));
            }
            if (onlineMail) {
                SmtpMessage m;
                m.subject = "QMark backup — " + QDateTime::currentDateTime().toString("yyyy-MM-dd hh:mm:ss");
                m.bodyPlain = Tr::trS("Backup created: %1").arg(fileName);
                m.recipients = recipients;
                m.attachments.append({ fileName, bundle });
                QString merr;
                if (!smtpSend(st, m, 60000, &merr))
                    report += "\n" + Tr::trS("E-mail failed: %1").arg(Tr::trS(merr));
            }
        }

        if (!guard) return;
        QMetaObject::invokeMethod(guard, [guard, report, path, stamp]() {
            if (!guard) return;
            guard->m_backupInProgress = false;
            if (!path.isEmpty())
                AppSettings::settings().setValue(AppSettings::keyBackupLastAt(), stamp);
            if (guard->m_autoUi.lblBackupStatus)
                guard->m_autoUi.lblBackupStatus->setText(report);
            guard->showStatus(report, 7000);
        }, Qt::QueuedConnection);
    }).detach();
}

// ── Auto-update check + install (worker thread + restart) ───────
void MainWindow::checkUpdatesNow()
{
    if (m_updateCheckInProgress) {
        showStatus(Tr::trS("Checking for updates…"), 3000);
        return;
    }
    m_updateCheckInProgress = true;
    if (m_autoUi.lblUpdateStatus) {
        m_autoUi.lblUpdateStatus->setText(Tr::trS("Checking for updates…"));
        m_autoUi.lblUpdateStatus->setStyleSheet("color: #2e7d32; background: transparent;");
    }

    QPointer<MainWindow> guard = this;
    std::thread([guard]() {
        UpdateInfo info;
        QString err;
        const bool ok = Updater::checkLatest(&info, &err);
        if (!guard) return;
        QMetaObject::invokeMethod(guard, [guard, ok, info, err]() {
            if (!guard) return;
            guard->m_updateCheckInProgress = false;
            if (!ok) {
                guard->noteUpdateCheckResult(Tr::trS("Update check failed: %1").arg(Tr::trS(err)), QString());
                return;
            }
            if (!info.newer) {
                guard->noteUpdateCheckResult(Tr::trS("Update check: up to date."), info.tag);
                return;
            }

            guard->noteUpdateCheckResult(Tr::trS("Update available: %1").arg(info.tag), info.tag);
            const int choice = QMessageBox::question(
                guard, Tr::trS("Updates"),
                Tr::trS("Download and apply update %1?").arg(info.tag),
                QMessageBox::Yes | QMessageBox::No, QMessageBox::Yes);
            if (choice != QMessageBox::Yes) return;

            // Download + swap on a worker thread, then restart.
            QPointer<MainWindow> g2 = guard;
            std::thread([g2, info]() {
                // Download next to the executable (same volume/filesystem)
                // so the final rename cannot fail across drives.
                const QString dest = QDir(
                    QCoreApplication::applicationDirPath())
                    .filePath(info.assetName + QStringLiteral(".part"));
                QString err;
                if (!Updater::download(info, dest, &err)) {
                    if (!g2) return;
                    QMetaObject::invokeMethod(g2, [g2, err]() {
                        if (!g2) return;
                        g2->noteUpdateCheckResult(Tr::trS("Update install failed: %1").arg(Tr::trS(err)), QString());
                    }, Qt::QueuedConnection);
                    return;
                }
                const QString applyErrTarget = QCoreApplication::applicationFilePath();
                QString aerr;
                const bool applied = Updater::apply(dest, applyErrTarget, &aerr);
                if (!g2) return;
                QMetaObject::invokeMethod(g2, [g2, applied, aerr]() {
                    if (!g2) return;
                    if (applied) {
                        g2->noteUpdateCheckResult(Tr::trS("Update installed — restarting…"), QString());
                        g2->close();
                    } else {
                        g2->noteUpdateCheckResult(Tr::trS("Update install failed: %1").arg(Tr::trS(aerr)), QString());
                    }
                }, Qt::QueuedConnection);
            }).detach();
        }, Qt::QueuedConnection);
    }).detach();
}

// ── Restore: swap <exe>.old (the previous release) back in ────────
void MainWindow::restorePreviousRelease()
{
    const QString target = QCoreApplication::applicationFilePath();
    if (!QFile::exists(target + ".old")) {
        QMessageBox::information(this, Tr::trS("Restore Previous Release"),
            Tr::trS("No previous release backup found. Install an update at least once to create one."));
        return;
    }

    const int choice = QMessageBox::question(
        this, Tr::trS("Restore Previous Release"),
        Tr::trS("Replace the current program with the previous release and restart?"),
        QMessageBox::Yes | QMessageBox::No, QMessageBox::Yes);
    if (choice != QMessageBox::Yes) return;

    QString err;
    if (!Updater::restore(target, &err)) {
        QMessageBox::warning(this, Tr::trS("Restore Previous Release"),
            Tr::trS("Could not restore the previous release: %1").arg(Tr::trS(err)));
        return;
    }
    // Updater::restore() relaunched the restored binary; leave now.
    close();
}

void MainWindow::noteUpdateCheckResult(const QString& result, const QString& detail)
{
    if (m_autoUi.lblUpdateStatus) {
        m_autoUi.lblUpdateStatus->setText(result);
        m_autoUi.lblUpdateStatus->setStyleSheet("color: #2e7d32; background: transparent;");
    }
    showStatus(result, 7000);
    AppSettings::settings().setValue(AppSettings::keyUpdateLastCheck(),
        QDateTime::currentDateTime().toString("yyyy-MM-dd hh:mm:ss"));
    if (!detail.isEmpty())
        AppSettings::settings().setValue(AppSettings::keyUpdateLastSeen(), detail);
}

// ── Scanner: sell the scanned product code ──────────────────────
void MainWindow::setScannerMode(bool on)
{
    m_scannerEnabled = on;
    AppSettings::settings().setValue(AppSettings::keyScanEnabled(), on);
    // Keep every visible toggle reflecting the shared state.
    auto syncCheck = [this](QCheckBox* cb) {
        if (!cb) return;
        cb->blockSignals(true);
        cb->setChecked(m_scannerEnabled);
        cb->blockSignals(false);
    };
    syncCheck(ui->chkScanner_sell);
    syncCheck(ui->chkScanner_item);
    syncCheck(ui->chkScanner_item_edit);
    syncCheck(ui->chkScanner_remove);
    syncCheck(m_autoUi.chkScanner);
    updateScannerHints();
}

// Forget any in-flight scanner burst (page change, sealed scan, ...).
void MainWindow::resetScannerBurst()
{
    m_scannerInBurst = false;
    m_scannerBuffer.clear();
    m_scannerLastNs = 0;
}

// Explain what scanner mode does right on the Add/Edit item pages, so an
// operator with the name field focused knows a scan targets the ID field.
void MainWindow::createScannerHintLabels()
{
    const QString hint = Tr::trS(
        "Scanner mode: scanning a barcode fills the ID field. A barcode "
        "that already has an item opens it for editing. The scanned code "
        "is put in the ID — not the name — field.");
    auto makeLabel = [hint](const QString& objectName) -> QLabel* {
        QLabel* lbl = new QLabel(hint);
        lbl->setObjectName(objectName);
        lbl->setWordWrap(true);
        lbl->setStyleSheet("color: #0078d4; background: transparent; font-size: 12px;");
        return lbl;
    };
    if (ui->pageAddItem && ui->pageAddItem->layout()) {
        m_lblScannerHintItem = makeLabel(QStringLiteral("lblScannerHint_item"));
        qobject_cast<QBoxLayout*>(ui->pageAddItem->layout())->insertWidget(1, m_lblScannerHintItem);
    }
    if (ui->pageEditItem && ui->pageEditItem->layout()) {
        m_lblScannerHintItemEdit = makeLabel(QStringLiteral("lblScannerHint_item_edit"));
        qobject_cast<QBoxLayout*>(ui->pageEditItem->layout())->insertWidget(1, m_lblScannerHintItemEdit);
    }
}

void MainWindow::updateScannerHints()
{
    const bool show = m_scannerEnabled;
    if (m_lblScannerHintItem)     m_lblScannerHintItem->setVisible(show);
    if (m_lblScannerHintItemEdit) m_lblScannerHintItemEdit->setVisible(show);
}

void MainWindow::startAddWithBarcode(const QString& code)
{
    // Clerks cannot add items; keep them on the POS page with a message.
    if (getCurrentUserRole() == Domain::User::Role::UserRole) {
        showStatus(Tr::trS("Barcode not found: %1").arg(code), 5000);
        return;
    }
    goToPage(2);
    if (ui->chkAutogenerateID_item) ui->chkAutogenerateID_item->setChecked(false);
    // The burst's first keystroke was not consumed, so it may have landed
    // in a focused field — clear the form, then pre-fill the barcode.
    if (ui->txtName_item)     ui->txtName_item->clear();
    if (ui->txtQuantity_item) ui->txtQuantity_item->clear();
    if (ui->txtPrice_item)    ui->txtPrice_item->clear();
    if (ui->txtCategory_item) ui->txtCategory_item->clear();
    if (ui->txtShelf_item)    ui->txtShelf_item->clear();
    if (ui->txtId_item) {
        ui->txtId_item->clear();
        ui->txtId_item->setText(code);
    }
    if (ui->txtName_item) ui->txtName_item->setFocus();
    showStatus(Tr::trS("Barcode %1 set as the item ID — enter the name and price.")
                   .arg(code), 5000);
}

void MainWindow::handleScannedCode(const QString& code)
{
    if (!m_isLoggedIn) {
        showStatus(Tr::trS("Scanning requires login."), 4000);
        return;
    }
    const QString c = code.trimmed();
    if (c.isEmpty()) return;

    const int page = ui->stackedWidget ? ui->stackedWidget->currentIndex() : -1;

    // The candidate first character of the burst was deliberately not
    // consumed (so a lone fast key is never swallowed); on the POS page it
    // therefore landed in the search box. Remove it now that the scan is
    // known to be real.
    if (page == 6 && ui->txtSearch_sell_page) {
        ui->txtSearch_sell_page->clear();
    }

    // Look the barcode up unambiguously: exact id, a single id-like
    // match, or a sole full-text match.
    std::optional<std::string> matchId;
    if (m_db.getItemById(c.toStdString()).has_value()) {
        matchId = c.toStdString();
    } else {
        const auto byId = m_db.searchItems(c.toStdString(), "id");
        if (byId.size() == 1) matchId = byId.front().id;
    }
    if (!matchId) {
        const auto all = m_db.searchItems(c.toStdString(), "");
        if (all.size() == 1) matchId = all.front().id;
    }

    if (matchId) {
        if (page == 2 || page == 3) {
            // The barcode already has an item assigned — open it on the
            // Edit page so its name/price can be changed.
            if (page == 2) {
                // The burst's first keystroke may have landed in the add
                // form; clear it before navigating so no stray character
                // is left behind for the next manual entry.
                if (ui->chkAutogenerateID_item) ui->chkAutogenerateID_item->setChecked(true);
                if (ui->txtName_item)       ui->txtName_item->clear();
                if (ui->txtQuantity_item)   ui->txtQuantity_item->clear();
                if (ui->txtPrice_item)      ui->txtPrice_item->clear();
                if (ui->txtCategory_item)   ui->txtCategory_item->clear();
                if (ui->txtShelf_item)      ui->txtShelf_item->clear();
                if (ui->txtId_item)         ui->txtId_item->clear();
                goToPage(3);
                if (ui->txtSearch_item_edit) ui->txtSearch_item_edit->setText(c);
                on_btnSearch_item_edit_clicked();
            }
            loadItemToEdit(QString::fromStdString(*matchId));
            showStatus(Tr::trS("Barcode found — edit item: %1")
                           .arg(QString::fromStdString(*matchId)), 5000);
            return;
        }
        sellItemById(*matchId);
        return;
    }

    // Barcode not assigned to any item: pre-fill the Add Item form so
    // the operator can enter a name and price for this barcode.
    startAddWithBarcode(c);
}

bool MainWindow::sellItemById(const std::string& itemId)
{
    auto itemOpt = m_db.getItemById(itemId);
    if (!itemOpt.has_value()) return false;
    const auto& item = itemOpt.value();

    if (item.quantity <= 0 || Domain::statusIsSoldOut(item.status)) {
        showStatus(Tr::trS("This item is out of stock."), 5000);
        return false;
    }

    auto result = BusinessLogic::sellItem(
        m_db, item.id, 1, m_currentUser.value_or(Domain::User{}).id);
    if (!result.isValid) {
        showStatus(translatedBusinessMessage(result.errorMessage), 6000);
        return false;
    }

    m_worklog.logEntry(WorklogEntry::ActionType::Sale, WorklogEntry::EntityType::Sale,
                       item.id, "Sold item: " + item.name);
    if (ui->stackedWidget && ui->stackedWidget->currentIndex() == 6) {
        refreshSellPage();   // live feedback on the POS page
    }
    refreshDashboard();
    showStatus(Tr::trS("Sold via scanner: %1").arg(QString::fromStdString(item.name)), 4000);
    return true;
}

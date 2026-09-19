#pragma once

// ─────────────────────────────────────────────────────────────────────
// translations.h — lightweight language layer for QMark.
//
// The app defaults to Polish ("pl"); the language can be switched to
// English ("en") in Preferences. This header-only module provides:
//   * Tr::trS("English text") → Polish text when language is "pl"
//   * Tr::applyLanguage(root) → walks a widget/action tree and retranslates
//     labels, buttons, checkboxes, line-edit placeholders, menus, actions.
//
// QComboBox items are intentionally NOT translated: they hold enum-like
// data values ("In Stock", "Admin", "All", ...) used by filtering and
// status logic.
// ─────────────────────────────────────────────────────────────────────

#include <QString>
#include <QHash>
#include <QObject>
#include <QWidget>
#include <QLabel>
#include <QPushButton>
#include <QCheckBox>
#include <QGroupBox>
#include <QRadioButton>
#include <QToolButton>
#include <QLineEdit>
#include <QComboBox>
#include <QAction>
#include <QMenu>
#include <QMenuBar>
#include <QSettings>

namespace Tr {

// Current language code: "pl" (default) or "en".
inline QString language()
{
    QSettings settings("QMark", "SchoolShop");
    return settings.value("settings/language", "pl").toString();
}

// ── plDict: English → Polish ───────────────────────────────────────
inline const QHash<QString, QString>& plDict()
{
    static const QHash<QString, QString> pl = {
        // ── Menus ────────────────────────────────────────────────
        { "Account", "Konto" },
        { "Items", "Przedmioty" },
        { "Sales", "Raporty" },
        { "Tools", "Narzędzia" },
        { "Sell", "Sprzedaż" },
        { "Language", "Język" },
        { "Language set to English", "Język ustawiony na angielski" },
        { "Language set to Polish", "Język ustawiony na polski" },

        // ── Menu actions ─────────────────────────────────────────
        { "Exit", "Wyjdź" },
        { "Log Out", "Wyloguj się" },
        { "Log In", "Zaloguj się" },
        { "Register User", "Zarejestruj użytkownika" },
        { "Sell Item (POS)", "Sprzedaj przedmiot (POS)" },
        { "Resupply", "Dostawa" },
        { "Add Item", "Dodaj przedmiot" },
        { "Edit Item", "Edytuj przedmiot" },
        { "Remove Item", "Usuń przedmiot" },
        { "Undo Removed Items", "Cofnij usunięte przedmioty" },
        { "Manage Categories", "Zarządzaj kategoriami" },
        { "Manage Shelves", "Zarządzaj półkami" },
        { "Sales History", "Historia sprzedaży" },
        { "Sales Report", "Raport sprzedaży" },
        { "Database Selection", "Wybór bazy danych" },
        { "Create Database", "Utwórz bazę danych" },
        { "Accounts", "Konta" },
        { "Preferences", "Ustawienia" },
        { "Worklog Statistics", "Statystyki dziennika" },
        { "Troubleshoot", "Rozwiązywanie problemów" },

        // ── Login page ───────────────────────────────────────────
        { "Point of Sale System", "System punktu sprzedaży" },
        { "Username", "Nazwa użytkownika" },
        { "Password", "Hasło" },
        { "Show password", "Pokaż hasło" },
        { "LOG IN", "ZALOGUJ SIĘ" },

        // ── Dashboard ────────────────────────────────────────────
        { "SELL", "SPRZEDAJ" },
        { "START SELLING", "ROZPOCZNIJ SPRZEDAŻ" },
        
        { "Total Items", "Liczba przedmiotów" },
        { "Items Sold", "Sprzedano sztuk" },
        { "Revenue", "Przychód" },
        { "Recent Sales", "Ostatnie sprzedaże" },

        // ── Add / Edit / Remove item pages ───────────────────────
        { "Add New Item", "Dodaj nowy przedmiot" },
        { "Edit Item - Search", "Edytuj przedmiot - Szukaj" },
        { "Remove Item - Search", "Usuń przedmiot - Szukaj" },
        { "Item Name *", "Nazwa przedmiotu *" },
        { "ID (auto-generated if checked)", "ID (generowane automatycznie, jeśli zaznaczone)" },
        { "Auto-generate ID", "Generuj ID automatycznie" },
        { "Check ID", "Sprawdź ID" },
        { "Clear", "Wyczyść" },
        { "Quantity *", "Ilość *" },
        { "Price ($) *", "Cena ($) *" },
        { "Price (zł) *", "Cena (zł) *" },
        { "Category", "Kategoria" },
        { "Shelf / Location", "Półka / Miejsce" },
        { "Add Item", "Dodaj przedmiot" },
        { "Edit Item", "Edytuj przedmiot" },
        { "Remove Item", "Usuń przedmiot" },
        { "Search", "Szukaj" },
        { "Search by name, ID, category, shelf or status...", "Szukaj po nazwie, ID, kategorii, półce lub statusie..." },
        { "Search by name or ID...", "Szukaj po nazwie lub ID..." },
        { "Status", "Status" },
        { "In Stock", "W magazynie" },
        { "Low Stock", "Niski stan" },
        { "Out of Stock", "Brak w magazynie" },
        { "Sold Out", "Wyprzedane" },
        { "Select status", "Wybierz status" },

        // ── Sell (POS) page ──────────────────────────────────────
        { "Select an item to sell", "Wybierz przedmiot do sprzedaży" },
        { "Search items...", "Szukaj przedmiotów..." },
        { "Search", "Szukaj" },
        { "Sales log", "Dziennik sprzedaży" },
        { "Sales Log", "Dziennik sprzedaży" },
        { "Undo Sale", "Cofnij sprzedaż" },
        { "Refresh", "Odśwież" },
        { "Enable sell keybinds (Alt+1-9, Alt+0, Alt+letters, arrows, Enter; Alt+X cancels the last sale)", "Włącz skróty klawiszowe sprzedaży (Alt+1-9, Alt+0, Alt+litery, strzałki, Enter; Alt+X anuluje ostatnią sprzedaż)" },
        { "No sales recorded yet.", "Brak zarejestrowanych sprzedaży." },
        { "Simple view (show today's statistics)", "Widok prosty (pokaż dzisiejsze statystyki)" },
        { "TODAY'S STATISTICS", "DZISIEJSZE STATYSTYKI" },
        { "Sales: 0", "Sprzedaż: 0" },
        { "Revenue: 0,00 zł", "Przychód: 0,00 zł" },
        { "Items sold: 0", "Sprzedane sztuki: 0" },
        { "Top item: -", "Najlepszy przedmiot: -" },
        { "Trend: -", "Trend: -" },
        { "Sales: ", "Sprzedaż: " },
        { "Revenue: ", "Przychód: " },
        { "Items sold: ", "Sprzedane sztuki: " },
        { "Top item: ", "Najlepszy przedmiot: " },
        { "Trend: ", "Trend: " },
        { "vs yesterday", "w porównaniu z wczoraj" },
        { "no sales yesterday", "brak sprzedaży wczoraj" },
        { "sold %1 for %2", "Sprzedano %1 za %2" },
        { "… and %1 more", "… i %1 więcej" },
        { "No sales today.", "Brak dzisiejszych sprzedaży." },

        // ── Categories page ──────────────────────────────────────
        { "Category Name", "Nazwa kategorii" },
        { "ID", "ID" },
        { "Add", "Dodaj" },
        { "Edit", "Edytuj" },
        { "Remove", "Usuń" },

        // ── Shelves page ─────────────────────────────────────────
        { "Shelf Name", "Nazwa półki" },

        // ── Sales history ────────────────────────────────────────
        { "Search sales...", "Szukaj sprzedaży..." },
        { "Export CSV", "Eksportuj CSV" },
        { "Simple view", "Widok prosty" },

        // ── Reports ──────────────────────────────────────────────
        { "Generate Report", "Generuj raport" },
        { "Export Report", "Eksportuj raport" },
        { "Export PDF", "Eksportuj PDF" },
        { "PDF Files", "Pliki PDF" },
        { "Generate the report first.", "Najpierw wygeneruj raport." },
        { "PDF report saved.", "Raport PDF zapisany." },
        { "Could not save the PDF report.", "Nie udało się zapisać raportu PDF." },
        { "SALES REPORT", "RAPORT SPRZEDAŻY" },
        { "Generated: ", "Wygenerowano: " },
        { "Total Transactions: ", "Całkowita liczba transakcji: " },
        { "Total Items Sold: ", "Całkowita liczba sprzedanych sztuk: " },
        { "Items in stock: ", "Przedmioty w magazynie: " },
        { "Low Stock: ", "Niski stan: " },
        { "Out of Stock: ", "Brak w magazynie: " },
        { "TRENDS", "TRENDY" },
        { "Revenue by day (last 7 days):", "Przychód wg dnia (ostatnie 7 dni):" },
        { "Trend vs yesterday: ", "Trend względem wczoraj: " },
        { "(up)", "(wzrost)" },
        { "(down)", "(spadek)" },
        { "Trend: no sales yesterday (new activity today)", "Trend: brak sprzedaży wczoraj (nowa aktywność dzisiaj)" },
        { "Top selling items:", "Najlepiej sprzedające się przedmioty:" },
        { "(no sales yet)", "(brak sprzedaży)" },
        { "(no sales recorded yet)", "(brak zarejestrowanych sprzedaży)" },
        { "pcs, ", "szt., " },
        { "pcs", "szt." },
        { "Top sellers (staff):", "Najlepsi sprzedawcy (personel):" },
        { "tx, ", "trans., " },
        { "Low stock items needing replenishment:", "Przedmioty o niskim stanie wymagające uzupełnienia:" },
        { "Today's Sales: ", "Dzisiejsza sprzedaż: " },
        { "Today's Revenue: ", "Dzisiejszy przychód: " },
        { "Today's Items Sold: ", "Dzisiejsze sprzedane sztuki: " },
        { "Today's Top Seller: ", "Dzisiejszy najlepszy sprzedawca: " },
        { "Today's Top Item: ", "Dzisiejszy najlepszy przedmiot: " },
        { "Today's Busiest Hour: ", "Dzisiejsza godzina szczytu: " },
        { "sales", "sprzedaży" },
        { "Top Selling Items", "Najlepiej sprzedające się przedmioty" },
        { "Shop activity by hour", "Aktywność sklepu wg godziny" },
        { "Other", "Inne" },

        // ── Database config ───────────────────────────────────────
        { "Items Database", "Baza danych przedmiotów" },
        { "Users Database", "Baza danych użytkowników" },
        { "Browse", "Przeglądaj" },
        { "Load Configuration", "Wczytaj konfigurację" },
        { "Save as Default", "Zapisz jako domyślne" },
        { "Test Connection", "Testuj połączenie" },
        { "Create New Database", "Utwórz nową bazę danych" },

        // ── Accounts page ─────────────────────────────────────────
        { "Change Role", "Zmień rolę" },
        { "Change Password", "Zmień hasło" },
        { "Delete Account", "Usuń konto" },
        { "Search users...", "Szukaj użytkowników..." },

        // ── Preferences page ──────────────────────────────────────
        { "Enable Worklog", "Włącz dziennik" },
        { "Enable Telemetry", "Włącz telemetrię" },
        { "Language", "Język" },
        { "Currency", "Waluta" },
        { "Sell keybinds", "Skróty klawiszowe sprzedaży" },
        { "Enable sell keybinds", "Włącz skróty klawiszowe sprzedaży" },
        { "Save Preferences", "Zapisz ustawienia" },
        { "Reset to Defaults", "Przywróć domyślne" },
        { "Convert All Prices...", "Przelicz wszystkie ceny..." },
        { "Switching currency does not convert prices.", "Zmiana waluty nie przelicza cen." },
        { "Only SuperAdmin can change the currency.", "Tylko SuperAdmin może zmienić walutę." },

        // ── Worklog page ─────────────────────────────────────────
        { "Export Worklog", "Eksportuj dziennik" },
        { "WORKLOG SESSION STATISTICS", "STATYSTYKI SESJI DZIENNIKA" },
        { "Session Start: ", "Początek sesji: " },
        { "Items Added: ", "Dodane przedmioty: " },
        { "Items Edited: ", "Edytowane przedmioty: " },
        { "Items Removed: ", "Usunięte przedmioty: " },
        { "Sales Recorded: ", "Zarejestrowane sprzedaże: " },
        { "Categories Added: ", "Dodane kategorie: " },
        { "Categories Edited: ", "Edytowane kategorie: " },
        { "Categories Removed: ", "Usunięte kategorie: " },
        { "Shelves Added: ", "Dodane półki: " },
        { "Shelves Edited: ", "Edytowane półki: " },
        { "Shelves Removed: ", "Usunięte półki: " },
        { "Users Added: ", "Dodani użytkownicy: " },
        { "Users Edited: ", "Edytowani użytkownicy: " },
        { "Users Removed: ", "Usunięci użytkownicy: " },
        { "Export", "Eksport" },
        { "Worklog exported.", "Dziennik wyeksportowany." },

        // ── Troubleshoot page ────────────────────────────────────
        { "Test DB Connection", "Testuj połączenie z bazą" },
        { "View Logs", "Zobacz logi" },
        { "Export Diagnostics", "Eksportuj diagnostykę" },
        { "Troubleshooting", "Rozwiązywanie problemów" },
        { "DB Test", "Test bazy danych" },
        { "Connected.", "Połączono." },
        { "Not connected.", "Brak połączenia." },
        { "Logs", "Logi" },
        { "Log files are located in:", "Pliki logów znajdują się w:" },
        { "Diagnostics exported.", "Diagnostyka wyeksportowana." },
        { "=== QMark Diagnostics ===", "=== Diagnostyka QMark ===" },
        { "Date: ", "Data: " },
        { "DB Connected: ", "Połączenie z BD: " },
        { "Logged In: ", "Zalogowano: " },
        { "User: ", "Użytkownik: " },
        { "Role: ", "Rola: " },
        { "Telemetry: ", "Telemetria: " },
        { "Worklog: ", "Dziennik: " },
        { "Currency: ", "Waluta: " },
        { "Language: ", "Język: " },
        { "Text Files", "Pliki tekstowe" },
        { "Yes", "Tak" },
        { "No", "Nie" },
        { "On", "Wł." },
        { "Off", "Wył." },

        // ── Register page ─────────────────────────────────────────
        { "Register New User", "Zarejestruj nowego użytkownika" },
        { "SuperAdmin only", "Tylko SuperAdmin" },
        { "Confirm Password", "Potwierdź hasło" },
        { "Show passwords", "Pokaż hasła" },
        { "REGISTER", "ZAREJESTRUJ" },
        { "Password strength", "Siła hasła" },
        { "Weak", "Słabe" },
        { "Medium", "Średnie" },
        { "Strong", "Mocne" },
        { "Very strong", "Bardzo mocne" },
        { "Role Info", "Informacje o roli" },
        { "ℹ Role Info", "ℹ Info o roli" },
        { "Roles and their permissions:", "Role i ich uprawnienia:" },
        { "Clerk", "Sprzedawca" },
        { "Admin", "Administrator" },
        { "SuperAdmin", "Superadministrator" },
        { "Clerk role: can sell items, view the sales log and undo sales only.", "Rola Sprzedawca: może sprzedawać przedmioty, przeglądać dziennik sprzedaży i cofać sprzedaż." },
        { "Admin role: manages items, shelves, categories, prices, reports and worklogs.", "Rola Administrator: zarządza przedmiotami, półkami, kategoriami, cenami, raportami i dziennikami." },
        { "SuperAdmin role: everything an Admin can do, plus user accounts, roles, passwords and currency.", "Rola Superadministrator: wszystko co Administrator, plus konta użytkowników, role, hasła i walutę." },

        // ── Common dialog strings ────────────────────────────────
        { "Please enter username and password.", "Wprowadź nazwę użytkownika i hasło." },
        { "Invalid username or password.", "Nieprawidłowa nazwa użytkownika lub hasło." },
        { "Login Required", "Wymagane logowanie" },
        { "Please log in first.", "Najpierw się zaloguj." },
        { "Access Denied", "Odmowa dostępu" },
        { "Login", "Logowanie" },
        { "Register", "Rejestracja" },
        { "Preferences saved.", "Ustawienia zapisano." },
        { "Preferences reset to defaults.", "Przywrócono ustawienia domyślne." },
        { "No sales to undo.", "Brak sprzedaży do cofnięcia." },
        { "Undo this sale?", "Cofnąć tę sprzedaż?" },
        { "Sale not found.", "Nie znaleziono sprzedaży." },
        { "The item was removed and could not be restored.", "Przedmiot został usunięty i nie można go przywrócić." },
        { "Sale undone. Stock restored.", "Sprzedaż cofnięta. Stan magazynu przywrócony." },
        { "Undo Sale", "Cofnij sprzedaż" },
        { "Undo", "Cofnij" },
        { "Undo All", "Cofnij wszystko" },
        { "Undo Remove", "Cofnij usuwanie" },
        { "Item restored.", "Przywrócono przedmiot." },
        { "%1 item(s) restored.", "Przywrócono %1 przedmiot(y)." },
        { "Restore Removed Items", "Przywróć usunięte przedmioty" },
        { "Restore Selected", "Przywróć zaznaczone" },
        { "Restore All", "Przywróć wszystko" },
        { "Item sold successfully!", "Przedmiot sprzedany pomyślnie!" },
        { "Sell Item", "Sprzedaj przedmiot" },
        { "Quantity to sell:", "Ilość do sprzedania:" },
        { "Sale Error", "Błąd sprzedaży" },
        { "This item is out of stock.", "Ten przedmiot jest niedostępny." },
        { "Item not found.", "Nie znaleziono przedmiotu." },
        { "Quantity updated.", "Zaktualizowano ilość." },
        { "Logged in as ", "Zalogowano jako " },
        { "Logged out", "Wylogowano" },
        { "Total Revenue: ", "Całkowity przychód: " },
        { "Search Total: ", "Wynik wyszukiwania: " },
        { "Qty: ", "Ilość: " },
        { "Current: ", "Stan: " },
        { "Sale ", "Sprzedaż " },
        { "Item: ", "Przedmiot: " },
        { "By: ", "Przez: " },
        { "ID: ", "ID: " },
        { "Quantity updated to", "Zaktualizowano ilość do" },
        { "Quantity set to", "Ustawiono ilość na" },
        { "Adjust quantity:", "Dostosuj ilość:" },
        { "Selected Item", "Wybrany przedmiot" },
        { "No item selected", "Nie wybrano przedmiotu" },
        { "Search and select an item from the list", "Wyszukaj i wybierz przedmiot z listy" },
        { "Search Items", "Szukaj przedmiotów" },
        { "Type item name to search...", "Wpisz nazwę przedmiotu, aby wyszukać..." },
        { "Category: ", "Kategoria: " },
        { "Shelf: ", "Półka: " },
        { "APPLY", "ZASTOSUJ" },
        { "← Back", "← Wstecz" },
        { "Failed to update. Try again.", "Nie udało się zaktualizować. Spróbuj ponownie." },
        { "Enter conversion factor (new price = old price × factor):", "Podaj współczynnik przeliczenia (nowa cena = stara cena × współczynnik):" },
        { "item(s) updated.", "przedmiot(y) zaktualizowano." },

        // ── First-run setup ───────────────────────────────────────
        { "Welcome to QMark", "Witaj w QMark" },
        { "No user accounts found.\nCreate a SuperAdmin account to get started.", "Nie znaleziono kont użytkowników.\nUtwórz konto SuperAdmin, aby rozpocząć." },
        { "Confirm password", "Potwierdź hasło" },
        { "CREATE ACCOUNT", "UTWÓRZ KONTO" },
        { "Setup Complete", "Konfiguracja zakończona" },
        { "SuperAdmin account created!\n\nYou can now log in.", "Konto SuperAdmin utworzone!\n\nMożesz się teraz zalogować." },
        { "All fields are required.", "Wszystkie pola są wymagane." },
        { "Passwords do not match.", "Hasła nie są zgodne." },
        { "Password must be at least 4 characters.", "Hasło musi mieć co najmniej 4 znaki." },
        { "Username already exists.", "Użytkownik już istnieje." },
        { "Failed to hash password. Try again.", "Nie udało się zahaszować hasła. Spróbuj ponownie." },
        { "Failed to create user. Try again.", "Nie udało się utworzyć użytkownika. Spróbuj ponownie." },
        { "User registered successfully.", "Użytkownik zarejestrowany pomyślnie." },
        { "Only SuperAdmin can register new users.", "Tylko SuperAdmin może rejestrować nowych użytkowników." },
    };
    return pl;
}

// ── enDict: Polish → English (reverse of plDict) ───────────────────
// Lets a switch back to English restore the original labels instead of
// leaving stale Polish text behind. First match wins for any values
// that happen to collide.
inline const QHash<QString, QString>& enDict()
{
    static const QHash<QString, QString> en = [] {
        QHash<QString, QString> m;
        const auto& pl = plDict();
        for (auto it = pl.constBegin(); it != pl.constEnd(); ++it) {
            if (!m.contains(it.value())) m.insert(it.value(), it.key());
        }
        return m;
    }();
    return en;
}

// Translate a single string. In "pl" mode English → Polish; in "en"
// mode Polish (or stale-translated) text is mapped back to the original
// English, and anything else passes through unchanged.
inline QString trS(const QString& text)
{
    if (language() == "pl") return plDict().value(text, text);
    return enDict().value(text, text);
}

// Recursively retranslate an object tree. Safe to call repeatedly.
inline void applyLanguage(QObject* root)
{
    if (!root) return;

    if (auto* bar = qobject_cast<QMenuBar*>(root)) {
        for (QAction* a : bar->actions()) {
            if (a->menu()) applyLanguage(a->menu());
            else a->setText(trS(a->text()));
        }
        for (QObject* child : bar->children()) applyLanguage(child);
        return;
    }

    if (auto* menu = qobject_cast<QMenu*>(root)) {
        menu->setTitle(trS(menu->title()));
        for (QAction* a : menu->actions()) {
            if (a->menu()) applyLanguage(a->menu());
            else a->setText(trS(a->text()));
        }
        for (QObject* child : menu->children()) applyLanguage(child);
        return;
    }

    if (auto* action = qobject_cast<QAction*>(root)) {
        action->setText(trS(action->text()));
        return;
    }

    if (auto* lbl = qobject_cast<QLabel*>(root)) {
        lbl->setText(trS(lbl->text()));
    } else if (auto* btn = qobject_cast<QPushButton*>(root)) {
        btn->setText(trS(btn->text()));
    } else if (auto* cb = qobject_cast<QCheckBox*>(root)) {
        cb->setText(trS(cb->text()));
    } else if (auto* grp = qobject_cast<QGroupBox*>(root)) {
        grp->setTitle(trS(grp->title()));
    } else if (auto* rb = qobject_cast<QRadioButton*>(root)) {
        rb->setText(trS(rb->text()));
    } else if (auto* tb = qobject_cast<QToolButton*>(root)) {
        tb->setText(trS(tb->text()));
    } else if (auto* le = qobject_cast<QLineEdit*>(root)) {
        le->setPlaceholderText(trS(le->placeholderText()));
    }
    // QComboBox items are intentionally left untranslated
    // (statuses/search-fields/roles are enum-like data values).

    for (QObject* child : root->children()) {
        applyLanguage(child);
    }
}

} // namespace Tr
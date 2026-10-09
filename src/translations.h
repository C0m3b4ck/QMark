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
        { "Enable sell keybinds (Alt+keys, arrows, Enter; Alt+X cancels the last sale)", "Włącz skróty klawiszowe sprzedaży (Alt+klawisze, strzałki, Enter; Alt+X anuluje ostatnią sprzedaż)" },
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

        // ── Automation & Remote page ─────────────────────────────
        { "Automation & Remote", "Automatyzacja i dostęp zdalny" },
        { "Scanner mode", "Tryb skanera" },
        { "Enable barcode scanner mode (keyboard input)", "Włącz tryb skanera kodów kreskowych (wejście klawiaturowe)" },
        { "Scans are read from any USB/HID barcode scanner quickly and sold automatically (POS page).", "Odczyty z dowolnego skanera USB/HID są wykrywane błyskawicznie i sprzedawane automatycznie (strona POS)." },
        { "Remote dashboard", "Pulpit zdalny" },
        { "Enable remote dashboard for SuperAdmin", "Włącz zdalny pulpit dla SuperAdmin" },
        { "A read-only dashboard + JSON API on the local network for remote SuperAdmin access.", "Tylko-do-odczytu pulpit + JSON API w sieci lokalnej do zdalnego dostępu SuperAdmin." },
        { "Port", "Port" },
        { "Access token (long & random)", "Token dostępu (długi i losowy)" },
        { "Generate token", "Generuj token" },
        { "Dashboard URL: ", "Adres pulpitu: " },
        { "Security warning: the token is the only gate — keep it secret.", "Ostrzeżenie: token jest jedyną bramką — trzymaj go w tajemnicy." },
        { "E-mail summary", "Podsumowanie e-mail" },
        { "Enable scheduled e-mail summaries", "Włącz zaplanowane podsumowania e-mail" },
        { "SMTP host", "Serwer SMTP" },
        { "SMTP port", "Port SMTP" },
        { "Security", "Bezpieczeństwo" },
        { "Sender address", "Adres nadawcy" },
        { "Recipients (one per line)", "Odbiorcy (jeden na linię)" },
        { "Send daily summary", "Wyślij dzienne podsumowanie" },
        { "Send monthly summary", "Wyślij miesięczne podsumowanie" },
        { "Day of month", "Dzień miesiąca" },
        { "Attach database backup to each summary", "Dołącz kopię zapasową bazy do każdego podsumowania" },
        { "Send test e-mail", "Wyślij e-mail testowy" },
        { "Save Automation Settings", "Zapisz ustawienia automatyzacji" },
        { "Automation settings saved.", "Zapisano ustawienia automatyzacji." },
        { "Backups", "Kopie zapasowe" },
        { "Backup folder", "Folder kopii zapasowych" },
        { "Keep last N backups", "Zachowaj ostatnich N kopii" },
        { "Scheduled backup", "Planowana kopia zapasowa" },
        { "Time of day", "Godzina" },
        { "Send a copy online via e-mail", "Wyślij kopię online e-mailem" },
        { "Upload a copy to a custom server (HTTP PUT)", "Prześlij kopię na własny serwer (HTTP PUT)" },
        { "Server URL (or {filename} template)", "Adres URL serwera (lub wzorzec {filename})" },
        { "Bearer token (optional)", "Token dostępu (opcjonalnie)" },
        { "Back up now", "Wykonaj kopię teraz" },
        { "Last backup: none yet", "Ostatnia kopia: brak" },
        { "Backup created: %1", "Utworzono kopię: %1" },
        { "Online upload failed: %1", "Przesyłanie online nie powiodło się: %1" },
        { "Auto-update", "Automatyczne aktualizacje" },
        { "Enable automatic update checks from GitHub", "Włącz automatyczne sprawdzanie aktualizacji z GitHub" },
        { "Check for updates now", "Sprawdź aktualizacje teraz" },
        { "Update check: up to date.", "Aktualizacje: brak nowej wersji." },
        { "Update available: %1", "Dostępna aktualizacja: %1" },
        { "Update check failed: %1", "Sprawdzanie aktualizacji nie powiodło się: %1" },
        { "Update installed — restarting…", "Aktualizacja zainstalowana — ponowne uruchamianie…" },
        { "Update install failed: %1", "Instalacja aktualizacji nie powiodła się: %1" },
        { "Downloading update…", "Pobieranie aktualizacji…" },
        { "Apply and restart now?", "Zastosować i uruchomić ponownie teraz?" },
        { "Download and apply update %1?", "Pobrać i zastosować aktualizację %1?" },
        { "Updates", "Aktualizacje" },

        // ── Scanner / general status messages ────────────────────
        { "Scanner mode enabled.", "Tryb skanera włączony." },
        { "Scanner mode disabled.", "Tryb skanera wyłączony." },
        { "Sold via scanner: %1", "Sprzedano przez skaner: %1" },
        { "Scanned code not found: %1", "Nie znaleziono kodu: %1" },
        { "Scanning requires login.", "Skanowanie wymaga zalogowania." },
        { "Running on port %1", "Działa na porcie %1" },
        { "Remote token must be at least %1 characters.", "Token zdalny musi mieć co najmniej %1 znaków." },
        { "Dashboard stopped.", "Pulpit zatrzymany." },
        { "Dashboard error: %1", "Błąd pulpitu: %1" },
        { "Last e-mail: none yet", "Ostatni e-mail: brak" },
        { "Daily summary sent.", "Wysłano dzienne podsumowanie." },
        { "Monthly summary sent.", "Wysłano miesięczne podsumowanie." },
        { "E-mail failed: %1", "Błąd e-mail: %1" },
        { "Test e-mail sent to %1", "E-mail testowy wysłano do %1" },
        { "E-mail settings are incomplete.", "Ustawienia e-mail są niekompletne." },
        { "Sending e-mail…", "Wysyłanie e-mail…" },
        { "E-mail sending is already in progress.", "Wysyłanie e-maila już trwa." },
        { "Last e-mail: %1", "Ostatni e-mail: %1" },
        { "Backup already in progress.", "Kopia zapasowa już trwa." },
        { "Creating backup…", "Tworzenie kopii zapasowej…" },
        { "Last backup: %1", "Ostatnia kopia: %1" },
        { "Checking for updates…", "Sprawdzanie aktualizacji…" },

        // ── Daily / monthly summary (e-mail content) ─────────────
        { "DAILY SHOP SUMMARY", "DZIENNE PODSUMOWANIE SKLEPU" },
        { "MONTHLY SHOP SUMMARY", "MIESIĘCZNE PODSUMOWANIE SKLEPU" },
        { "Period: ", "Okres: " },
        { "Top Seller (today): ", "Najlepszy sprzedawca (dziś): " },
        { "Top Item (today): ", "Najlepszy przedmiot (dziś): " },
        { "Top Seller (month): ", "Najlepszy sprzedawca (miesiąc): " },
        { "Top Item (month): ", "Najlepszy przedmiot (miesiąc): " },
        { "End of summary", "Koniec podsumowania" },
        { "DAILY SUMMARY", "PODSUMOWANIE DZIENNE" },
        { "MONTHLY SUMMARY", "PODSUMOWANIE MIESIĘCZNE" },

        // ── App messages, dialogs and editors ────────────────────
        { "Settings", "Ustawienia" },
        { "Confirm", "Potwierdzenie" },
        { "Confirm Remove", "Potwierdź usunięcie" },
        { "Database", "Baza danych" },
        { "Create DB", "Utwórz bazę" },
        { "Test", "Test" },
        { "Shelf", "Półka" },
        { "All", "Wszystkie" },
        { "Name", "Nazwa" },
        { "Status", "Status" },
        { "Quantity", "Ilość" },
        { "Item Name", "Nazwa przedmiotu" },
        { "Price ($)", "Cena ($)" },
        { "ADD ITEM", "DODAJ PRZEDMIOT" },
        { "REMOVE", "USUŃ" },
        { "SAVE CHANGES", "ZAPISZ ZMIANY" },
        { "Clear Form", "Wyczyść formularz" },
        { "Clear ID", "Wyczyść ID" },
        { "Search item to edit...", "Szukaj przedmiotu do edycji..." },
        { "Search item to remove...", "Szukaj przedmiotu do usunięcia..." },
        { "Undo Add", "Cofnij dodanie" },
        { "Undo Edit", "Cofnij edycję" },
        { "Undo Last", "Cofnij ostatnie" },
        { "Undo Remove...", "Cofnij usunięcie..." },
        { "Automation & Remote", "Automatyka i zdalny dostęp" },
        { "Manage Accounts", "Zarządzaj kontami" },
        { "Database Configuration", "Konfiguracja baz danych" },
        { "Create New DBs", "Utwórz nowe bazy" },
        { "Load & Connect", "Wczytaj i połącz" },
        { "Items DB:", "Baza przedmiotów:" },
        { "Users DB:", "Baza użytkowników:" },
        { "Path to items.db", "Ścieżka do items.db" },
        { "Path to users.db", "Ścieżka do users.db" },
        { "Total Revenue: $0.00", "Całkowity przychód: $0.00" },
        { "Item added successfully.", "Przedmiot dodany pomyślnie." },
        { "Item updated successfully.", "Przedmiot zaktualizowany pomyślnie." },
        { "Item removed successfully.", "Przedmiot usunięty pomyślnie." },
        { "Failed to remove item.", "Nie udało się usunąć przedmiotu." },
        { "Enter an ID to check.", "Wprowadź ID, aby sprawdzić." },
        { "ID already exists.", "ID już istnieje." },
        { "ID is available.", "ID jest dostępne." },
        { "Remove item \"%1\"?", "Usunąć przedmiot \"%1\"?" },
        { "Category added.", "Kategoria dodana." },
        { "Category updated.", "Kategoria zaktualizowana." },
        { "Remove this category?", "Usunąć tę kategorię?" },
        { "Category removed.", "Kategoria usunięta." },
        { "Shelf added.", "Półka dodana." },
        { "Shelf updated.", "Półka zaktualizowana." },
        { "Remove this shelf?", "Usunąć tę półkę?" },
        { "Shelf removed.", "Półka usunięta." },
        { "Sales exported to %1", "Sprzedaż wyeksportowana do %1" },
        { "Report saved.", "Raport zapisany." },
        { "Please select both database files.", "Proszę wybrać oba pliki baz danych." },
        { "Connected to databases successfully.", "Połączono z bazami danych pomyślnie." },
        { "Failed to connect to databases.", "Nie udało się połączyć z bazami danych." },
        { "Default database configuration saved.", "Zapisano domyślną konfigurację baz danych." },
        { "Database connection is active.", "Połączenie z bazą danych jest aktywne." },
        { "No database connection.", "Brak połączenia z bazą danych." },
        { "New databases created and connected.", "Utworzono i połączono nowe bazy danych." },
        { "Failed to create databases.", "Nie udało się utworzyć baz danych." },
        { "Role changed.", "Rola zmieniona." },
        { "Failed to hash password.", "Nie udało się zahashować hasła." },
        { "Password changed.", "Hasło zmienione." },
        { "Account deletion not yet implemented.", "Usuwanie konta nie zostało jeszcze zaimplementowane." },
        { "Export Sales", "Eksport sprzedaży" },
        { "CSV Files (*.csv)", "Pliki CSV (*.csv)" },
        { "Export Report", "Eksport raportu" },
        { "Text Files (*.txt)", "Pliki tekstowe (*.txt)" },
        { "Select Items Database", "Wybierz bazę przedmiotów" },
        { "Select Users Database", "Wybierz bazę użytkowników" },
        { "SQLite DB (*.db)", "Baza SQLite (*.db)" },
        { "Change Role", "Zmień rolę" },
        { "Select new role:", "Wybierz nową rolę:" },
        { "New password:", "Nowe hasło:" },
        { "Export Worklog", "Eksport rejestru pracy" },
        { "Backup failed: %1", "Błąd kopii zapasowej: %1" },
        { "Scanner", "Skaner" },
        { "Barcode found — edit item: %1", "Znaleziono kod — edytuj przedmiot: %1" },
        { "Barcode not found — add a new item.", "Nie znaleziono kodu — dodaj nowy przedmiot." },
        { "Barcode not found: %1", "Nie znaleziono kodu: %1" },
        // ── Business-logic validation & flow messages (translated at the UI boundary) ──
        { "Item name cannot be empty", "Nazwa przedmiotu nie może być pusta" },
        { "Item ID cannot be empty", "ID przedmiotu nie może być puste" },
        { "Quantity cannot be negative", "Ilość nie może być ujemna" },
        { "Price must be greater than zero", "Cena musi być większa od zera" },
        { "Item status cannot be empty", "Status przedmiotu nie może być pusty" },
        { "Sale ID cannot be empty", "ID sprzedaży nie może być puste" },
        { "Sale item ID cannot be empty", "ID przedmiotu sprzedaży nie może być puste" },
        { "Quantity sold must be at least 1", "Sprzedana ilość musi wynosić co najmniej 1" },
        { "Quantity must be at least 1", "Ilość musi wynosić co najmniej 1" },
        { "Category name cannot be empty", "Nazwa kategorii nie może być pusta" },
        { "Category ID cannot be empty", "ID kategorii nie może być puste" },
        { "Shelf name cannot be empty", "Nazwa półki nie może być pusta" },
        { "Shelf ID cannot be empty", "ID półki nie może być puste" },
        { "Username cannot be empty", "Nazwa użytkownika nie może być pusta" },
        { "Password cannot be empty", "Hasło nie może być puste" },
        { "Username must be at least 3 characters", "Nazwa użytkownika musi mieć co najmniej 3 znaki" },
        { "Password must be at least 8 characters", "Hasło musi mieć co najmniej 8 znaków" },
        { "Username and password cannot be the same", "Nazwa użytkownika i hasło nie mogą być takie same" },
        { "Role must be selected", "Należy wybrać rolę" },
        { "Failed to hash password (out of memory?)", "Nie udało się zahashować hasła (brak pamięci?)" },
        { "Failed to register user", "Nie udało się zarejestrować użytkownika" },
        { "Failed to add item to database", "Nie udało się dodać przedmiotu do bazy danych" },
        { "Failed to add category to database", "Nie udało się dodać kategorii do bazy danych" },
        { "Failed to add shelf to database", "Nie udało się dodać półki do bazy danych" },
        { "Failed to update item in database", "Nie udało się zaktualizować przedmiotu w bazie danych" },
        { "Failed to update category in database", "Nie udało się zaktualizować kategorii w bazie danych" },
        { "Failed to update shelf in database", "Nie udało się zaktualizować półki w bazie danych" },
        { "Failed to complete sale (insufficient stock or DB error)", "Nie udało się zakończyć sprzedaży (niewystarczający stan lub błąd bazy danych)" },
        { "Item not found", "Nie znaleziono przedmiotu" },
        { "No database connection established", "Nie nawiązano połączenia z bazą danych" },
        { "No user logged in", "Nie zalogowano żadnego użytkownika" },
        { "Insufficient stock (available: %1)", "Niewystarczający stan (dostępne: %1)" },
        { "Access denied. Requires %1 role or higher.", "Odmowa dostępu. Wymagana rola %1 lub wyższa." },
        // ── Updater / network / e-mail / backup messages ──
        { "network error", "błąd sieci" },
        { "invalid GitHub response.", "Nieprawidłowa odpowiedź GitHub." },
        { "No releases found on GitHub.", "Brak wydań na GitHub." },
        { "No public releases found on GitHub.", "Brak publicznych wydań na GitHub." },
        { "Release has no %1 asset.", "Wydanie nie zawiera zasobu %1." },
        { "Refusing to download an update over an insecure connection.", "Odmowa pobrania aktualizacji przez niebezpieczne połączenie." },
        { "Downloaded update is missing or empty.", "Pobrana aktualizacja jest pusta lub niekompletna." },
        { "Could not back up the current executable.", "Nie udało się utworzyć kopii zapasowej bieżącego pliku wykonywalnego." },
        { "Could not install the update.", "Nie udało się zainstalować aktualizacji." },
        { "Restore previous release", "Przywróć poprzednią wersję" },
        { "Restore Previous Release", "Przywracanie poprzedniej wersji" },
        { "No previous release backup found. Install an update at least once to create one.", "Nie znaleziono kopii zapasowej poprzedniej wersji. Zainstaluj aktualizację co najmniej raz, aby ją utworzyć." },
        { "No previous release backup found.", "Nie znaleziono kopii zapasowej poprzedniej wersji." },
        { "Replace the current program with the previous release and restart?", "Zastąpić bieżący program poprzednią wersją i uruchomić go ponownie?" },
        { "Could not restore the previous release: %1", "Nie udało się przywrócić poprzedniej wersji: %1" },
        { "Could not restore the previous release.", "Nie udało się przywrócić poprzedniej wersji." },
        { "Could not set aside the current program.", "Nie udało się odłożyć bieżącego programu." },
        { "Cannot write %1", "Nie można zapisać %1" },
        { "Size mismatch: got %1, expected %2 bytes", "Niezgodność rozmiaru: otrzymano %1, oczekiwano %2 bajtów" },
        { "Backup server URL is not configured.", "Adres URL serwera kopii zapasowej nie jest skonfigurowany." },
        { "Cannot create backup folder: %1", "Nie można utworzyć folderu kopii zapasowej: %1" },
        { "Cannot write backup file: %1", "Nie można zapisać pliku kopii zapasowej: %1" },
        { "Cannot listen on port %1: %2", "Nie można nasłuchiwać na porcie %1: %2" },
        { "Remote dashboard listening on port %1", "Pulpit zdalny nasłuchuje na porcie %1" },
        { "Remote dashboard: rejected request from %1 (auth)", "Pulpit zdalny: odrzucono żądanie od %1 (auth)" },
        { "Connection failed: %1", "Połączenie nieudane: %1" },
        { "Sender e-mail address is not configured.", "Adres e-mail nadawcy nie jest skonfigurowany." },
        { "SMTP host is not configured.", "Serwer SMTP nie jest skonfigurowany." },
        { "No recipients configured.", "Brak skonfigurowanych odbiorców." },
        { "Unexpected SMTP greeting (code %1).", "Nieoczekiwane powitanie SMTP (kod %1)." },
        { "EHLO rejected (code %1).", "EHLO odrzucone (kod %1)." },
        { "EHLO (TLS) rejected (code %1).", "EHLO (TLS) odrzucone (kod %1)." },
        { "STARTTLS is required but the server did not offer it (code %1).", "Wymagany STARTTLS, ale serwer go nie zaoferował (kod %1)." },
        { "TLS handshake failed: %1", "Nie udało się wykonać uzgadniania TLS: %1" },
        { "TLS upgrade failed: %1", "Nie udało się uaktualnić do TLS: %1" },
        { "SMTP authentication failed (code %1).", "Uwierzytelnianie SMTP nieudane (kod %1)." },
        { "MAIL FROM rejected (code %1).", "MAIL FROM odrzucone (kod %1)." },
        { "RCPT TO rejected for %1 (code %2).", "RCPT TO odrzucone dla %1 (kod %2)." },
        { "DATA rejected (code %1).", "DATA odrzucone (kod %1)." },
        { "Message not accepted (code %1).", "Wiadomość niezaakceptowana (kod %1)." },
        { "Failed to finalize message data.", "Nie udało się sfinalizować danych wiadomości." },
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
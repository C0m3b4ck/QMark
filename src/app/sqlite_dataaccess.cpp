#include "sqlite_dataaccess.h"
#include "domain.h"
#include <SQLiteCpp/Database.h>
#include <SQLiteCpp/Statement.h>
#include <SQLiteCpp/Transaction.h>
#include <memory>
#include <mutex>
#include <stdexcept>
#include <algorithm>
#include <QDebug>

namespace DataAccess {

SQLiteDataAccess::SQLiteDataAccess()
    : m_connected(false)
{
}

SQLiteDataAccess::~SQLiteDataAccess()
{
    shutdown();
}

void SQLiteDataAccess::initialize(const std::string& itemsDb, const std::string& usersDb)
{
    std::lock_guard<std::mutex> lock(m_mutex);

    try {
        if (!itemsDb.empty()) {
            m_itemsDb = std::make_unique<SQLite::Database>(itemsDb, SQLite::OPEN_READWRITE | SQLite::OPEN_CREATE);
        }
        if (!usersDb.empty()) {
            m_usersDb = std::make_unique<SQLite::Database>(usersDb, SQLite::OPEN_READWRITE | SQLite::OPEN_CREATE);
        }

        if (m_itemsDb || m_usersDb) createTables();
        if (m_itemsDb || m_usersDb) createIndexes();

        m_connected = (m_itemsDb != nullptr) || (m_usersDb != nullptr);
    }
    catch (const std::exception& e) {
        shutdown();
        throw DataAccessException(std::string("Failed to initialize databases: ") + e.what());
    }
}

void SQLiteDataAccess::shutdown()
{
    std::lock_guard<std::mutex> lock(m_mutex);
    m_itemsDb.reset();
    m_usersDb.reset();
    m_connected = false;
}

bool SQLiteDataAccess::isConnected() const
{
    std::lock_guard<std::mutex> lock(m_mutex);
    return m_connected && m_itemsDb;
}

void SQLiteDataAccess::createTables()
{
    // Items table
    if (m_itemsDb) {
        m_itemsDb->exec(
            "CREATE TABLE IF NOT EXISTS items ("
            "id TEXT PRIMARY KEY,"
            "name TEXT NOT NULL,"
            "quantity INTEGER NOT NULL DEFAULT 0,"
            "price REAL NOT NULL DEFAULT 0.0,"
            "category TEXT DEFAULT '',"
            "shelf TEXT DEFAULT '',"
            "status TEXT NOT NULL DEFAULT 'In Stock',"
            "createdAt TEXT NOT NULL,"
            "updatedAt TEXT NOT NULL,"
            "deleted INTEGER NOT NULL DEFAULT 0"
            ");"
        );

        // Removed items table (for undo functionality)
        m_itemsDb->exec(
            "CREATE TABLE IF NOT EXISTS removed_items ("
            "id TEXT PRIMARY KEY,"
            "name TEXT NOT NULL,"
            "quantity INTEGER NOT NULL DEFAULT 0,"
            "price REAL NOT NULL DEFAULT 0.0,"
            "category TEXT DEFAULT '',"
            "shelf TEXT DEFAULT '',"
            "status TEXT NOT NULL,"
            "createdAt TEXT NOT NULL,"
            "updatedAt TEXT NOT NULL,"
            "deletedAt TEXT NOT NULL"
            ");"
        );

        // Sales table
        m_itemsDb->exec(
            "CREATE TABLE IF NOT EXISTS sales ("
            "id TEXT PRIMARY KEY,"
            "itemId TEXT NOT NULL,"
            "quantitySold INTEGER NOT NULL,"
            "unitPrice REAL NOT NULL,"
            "totalAmount REAL NOT NULL,"
            "soldBy TEXT NOT NULL,"
            "saleDate TEXT NOT NULL"
            ");"
        );

        // Categories table
        m_itemsDb->exec(
            "CREATE TABLE IF NOT EXISTS categories ("
            "id TEXT PRIMARY KEY,"
            "name TEXT NOT NULL UNIQUE"
            ");"
        );

        // Shelves table
        m_itemsDb->exec(
            "CREATE TABLE IF NOT EXISTS shelves ("
            "id TEXT PRIMARY KEY,"
            "name TEXT NOT NULL UNIQUE"
            ");"
        );
    }

    // Users table
    if (m_usersDb) {
        m_usersDb->exec(
            "CREATE TABLE IF NOT EXISTS users ("
            "id TEXT PRIMARY KEY,"
            "username TEXT NOT NULL UNIQUE,"
            "passwordHash TEXT NOT NULL,"
            "salt TEXT NOT NULL,"
            "role INTEGER NOT NULL DEFAULT 1,"
            "createdAt TEXT NOT NULL,"
            "lastLogin TEXT"
            ");"
        );
    }
}

void SQLiteDataAccess::createIndexes()
{
    if (m_itemsDb) {
        m_itemsDb->exec("CREATE INDEX IF NOT EXISTS idx_items_name ON items(name);");
        m_itemsDb->exec("CREATE INDEX IF NOT EXISTS idx_items_category ON items(category);");
        m_itemsDb->exec("CREATE INDEX IF NOT EXISTS idx_items_shelf ON items(shelf);");
        m_itemsDb->exec("CREATE INDEX IF NOT EXISTS idx_items_status ON items(status);");
        m_itemsDb->exec("CREATE INDEX IF NOT EXISTS idx_items_deleted ON items(deleted);");
        m_itemsDb->exec("CREATE INDEX IF NOT EXISTS idx_items_price ON items(price);");

        m_itemsDb->exec("CREATE INDEX IF NOT EXISTS idx_sales_itemId ON sales(itemId);");
        m_itemsDb->exec("CREATE INDEX IF NOT EXISTS idx_sales_soldBy ON sales(soldBy);");
        m_itemsDb->exec("CREATE INDEX IF NOT EXISTS idx_sales_saleDate ON sales(saleDate);");
    }
}

Domain::Item SQLiteDataAccess::rowToItem(const SQLite::Statement& stmt)
{
    Domain::Item item;
    item.id = stmt.getColumn(0).getText();
    item.name = stmt.getColumn(1).getText();
    item.quantity = stmt.getColumn(2).getInt();
    item.price = stmt.getColumn(3).getDouble();
    item.category = stmt.getColumn(4).getText();
    item.shelf = stmt.getColumn(5).getText();
    item.status = stmt.getColumn(6).getText();
    item.createdAt = stringToDateTime(stmt.getColumn(7).getText());
    item.updatedAt = stringToDateTime(stmt.getColumn(8).getText());
    return item;
}

Domain::Sale SQLiteDataAccess::rowToSale(const SQLite::Statement& stmt)
{
    Domain::Sale sale;
    sale.id = stmt.getColumn(0).getText();
    sale.itemId = stmt.getColumn(1).getText();
    sale.quantitySold = stmt.getColumn(2).getInt();
    sale.unitPrice = stmt.getColumn(3).getDouble();
    sale.totalAmount = stmt.getColumn(4).getDouble();
    sale.soldBy = stmt.getColumn(5).getText();
    sale.saleDate = stringToDateTime(stmt.getColumn(6).getText());
    return sale;
}

Domain::Category SQLiteDataAccess::rowToCategory(const SQLite::Statement& stmt)
{
    Domain::Category cat;
    cat.id = stmt.getColumn(0).getText();
    cat.name = stmt.getColumn(1).getText();
    return cat;
}

Domain::Shelf SQLiteDataAccess::rowToShelf(const SQLite::Statement& stmt)
{
    Domain::Shelf shelf;
    shelf.id = stmt.getColumn(0).getText();
    shelf.name = stmt.getColumn(1).getText();
    return shelf;
}

Domain::User SQLiteDataAccess::rowToUser(const SQLite::Statement& stmt)
{
    Domain::User user;
    user.id = stmt.getColumn(0).getText();
    user.username = stmt.getColumn(1).getText();
    user.passwordHash = stmt.getColumn(2).getText();
    user.salt = stmt.getColumn(3).getText();
    user.role = static_cast<Domain::User::Role>(stmt.getColumn(4).getInt());
    user.createdAt = stringToDateTime(stmt.getColumn(5).getText());
    user.lastLogin = stmt.getColumn(6).isNull() ? Domain::DateTime{} : stringToDateTime(stmt.getColumn(6).getText());
    return user;
}

std::string SQLiteDataAccess::dateTimeToString(const Domain::DateTime& dt)
{
    return Domain::toISOString(dt);
}

Domain::DateTime SQLiteDataAccess::stringToDateTime(const std::string& str)
{
    return Domain::fromISOString(str);
}

// ═══════════════════════════════════════════════════════════════════
// Item operations
// ═══════════════════════════════════════════════════════════════════

std::vector<Domain::Item> SQLiteDataAccess::getAllItems()
{
    std::lock_guard<std::mutex> lock(m_mutex);
    std::vector<Domain::Item> items;
    if (!m_itemsDb) return items;

    SQLite::Statement query(*m_itemsDb,
        "SELECT id, name, quantity, price, category, shelf, status, createdAt, updatedAt "
        "FROM items WHERE deleted = 0 ORDER BY name");
    while (query.executeStep()) {
        items.push_back(rowToItem(query));
    }
    return items;
}

std::optional<Domain::Item> SQLiteDataAccess::getItemById(const std::string& id)
{
    std::lock_guard<std::mutex> lock(m_mutex);
    if (!m_itemsDb) return std::nullopt;

    SQLite::Statement query(*m_itemsDb,
        "SELECT id, name, quantity, price, category, shelf, status, createdAt, updatedAt "
        "FROM items WHERE id = ? AND deleted = 0");
    query.bind(1, id);
    if (query.executeStep()) {
        return rowToItem(query);
    }
    return std::nullopt;
}

std::vector<Domain::Item> SQLiteDataAccess::searchItems(const std::string& term, const std::string& field)
{
    std::lock_guard<std::mutex> lock(m_mutex);
    std::vector<Domain::Item> items;
    if (!m_itemsDb) return items;

    std::string sql =
        "SELECT id, name, quantity, price, category, shelf, status, createdAt, updatedAt "
        "FROM items WHERE deleted = 0";

    if (field == "name") {
        sql += " AND name LIKE ?";
    } else if (field == "category") {
        sql += " AND category LIKE ?";
    } else if (field == "shelf") {
        sql += " AND shelf LIKE ?";
    } else if (field == "status") {
        sql += " AND status LIKE ?";
    } else if (field == "id") {
        sql += " AND CAST(id AS TEXT) LIKE ?";
    } else if (field == "price") {
        sql += " AND CAST(price AS TEXT) LIKE ?";
    } else {
        sql += " AND (name LIKE ? OR category LIKE ? OR shelf LIKE ? OR status LIKE ? "
               "OR CAST(id AS TEXT) LIKE ? OR CAST(price AS TEXT) LIKE ?)";
    }
    sql += " ORDER BY name";

    SQLite::Statement query(*m_itemsDb, sql);
    std::string pattern = "%" + term + "%";

    if (field == "name" || field == "category" || field == "shelf" ||
        field == "status" || field == "id" || field == "price") {
        query.bind(1, pattern);
    } else {
        for (int i = 1; i <= 6; ++i) {
            query.bind(i, pattern);
        }
    }

    while (query.executeStep()) {
        items.push_back(rowToItem(query));
    }
    return items;
}

bool SQLiteDataAccess::addItem(const Domain::Item& item)
{
    std::lock_guard<std::mutex> lock(m_mutex);
    if (!m_itemsDb) return false;
    try {
        SQLite::Transaction transaction(*m_itemsDb);
        SQLite::Statement query(*m_itemsDb,
            "INSERT INTO items (id, name, quantity, price, category, shelf, status, createdAt, updatedAt, deleted) "
            "VALUES (?, ?, ?, ?, ?, ?, ?, ?, ?, 0)");
        query.bind(1, item.id);
        query.bind(2, item.name);
        query.bind(3, item.quantity);
        query.bind(4, item.price);
        query.bind(5, item.category);
        query.bind(6, item.shelf);
        query.bind(7, item.status);
        query.bind(8, dateTimeToString(item.createdAt));
        query.bind(9, dateTimeToString(item.updatedAt));
        query.exec();
        transaction.commit();
        return true;
    }
    catch (const std::exception&) {
        return false;
    }
}

bool SQLiteDataAccess::updateItem(const Domain::Item& item)
{
    std::lock_guard<std::mutex> lock(m_mutex);
    if (!m_itemsDb) return false;
    try {
        SQLite::Transaction transaction(*m_itemsDb);
        SQLite::Statement query(*m_itemsDb,
            "UPDATE items SET name = ?, quantity = ?, price = ?, category = ?, shelf = ?, status = ?, updatedAt = ? "
            "WHERE id = ? AND deleted = 0");
        query.bind(1, item.name);
        query.bind(2, item.quantity);
        query.bind(3, item.price);
        query.bind(4, item.category);
        query.bind(5, item.shelf);
        query.bind(6, item.status);
        query.bind(7, dateTimeToString(item.updatedAt));
        query.bind(8, item.id);
        int rows = query.exec();
        transaction.commit();
        return rows > 0;
    }
    catch (const std::exception&) {
        return false;
    }
}

bool SQLiteDataAccess::removeItem(const std::string& id)
{
    std::lock_guard<std::mutex> lock(m_mutex);
    if (!m_itemsDb) return false;
    try {
        SQLite::Transaction transaction(*m_itemsDb);

        // Get the item data for undo
        SQLite::Statement selectQuery(*m_itemsDb,
            "SELECT id, name, quantity, price, category, shelf, status, createdAt, updatedAt "
            "FROM items WHERE id = ? AND deleted = 0");
        selectQuery.bind(1, id);
        Domain::Item item;
        bool found = false;
        if (selectQuery.executeStep()) {
            item = rowToItem(selectQuery);
            found = true;
        }

        if (!found) return false;

        // Move to removed_items table
        SQLite::Statement insertQuery(*m_itemsDb,
            "INSERT INTO removed_items (id, name, quantity, price, category, shelf, status, createdAt, updatedAt, deletedAt) "
            "VALUES (?, ?, ?, ?, ?, ?, ?, ?, ?, ?)");
        insertQuery.bind(1, item.id);
        insertQuery.bind(2, item.name);
        insertQuery.bind(3, item.quantity);
        insertQuery.bind(4, item.price);
        insertQuery.bind(5, item.category);
        insertQuery.bind(6, item.shelf);
        insertQuery.bind(7, item.status);
        insertQuery.bind(8, dateTimeToString(item.createdAt));
        insertQuery.bind(9, dateTimeToString(item.updatedAt));
        insertQuery.bind(10, dateTimeToString(Domain::now()));
        insertQuery.exec();

        // Mark as deleted in items table
        SQLite::Statement updateQuery(*m_itemsDb, "UPDATE items SET deleted = 1 WHERE id = ?");
        updateQuery.bind(1, id);
        updateQuery.exec();

        transaction.commit();
        return true;
    }
    catch (const std::exception&) {
        return false;
    }
}

std::vector<Domain::Item> SQLiteDataAccess::getRemovedItems()
{
    std::lock_guard<std::mutex> lock(m_mutex);
    std::vector<Domain::Item> items;
    if (!m_itemsDb) return items;

    SQLite::Statement query(*m_itemsDb,
        "SELECT id, name, quantity, price, category, shelf, status, createdAt, updatedAt "
        "FROM removed_items ORDER BY deletedAt DESC");
    while (query.executeStep()) {
        items.push_back(rowToItem(query));
    }
    return items;
}

bool SQLiteDataAccess::restoreItem(const std::string& id)
{
    std::lock_guard<std::mutex> lock(m_mutex);
    if (!m_itemsDb) return false;
    try {
        SQLite::Transaction transaction(*m_itemsDb);

        SQLite::Statement selectQuery(*m_itemsDb,
            "SELECT id, name, quantity, price, category, shelf, status, createdAt, updatedAt "
            "FROM removed_items WHERE id = ?");
        selectQuery.bind(1, id);
        Domain::Item item;
        bool found = false;
        if (selectQuery.executeStep()) {
            item = rowToItem(selectQuery);
            found = true;
        }

        if (!found) return false;

        // Insert back into items table
        SQLite::Statement insertQuery(*m_itemsDb,
            "INSERT OR REPLACE INTO items (id, name, quantity, price, category, shelf, status, createdAt, updatedAt, deleted) "
            "VALUES (?, ?, ?, ?, ?, ?, ?, ?, ?, 0)");
        insertQuery.bind(1, item.id);
        insertQuery.bind(2, item.name);
        insertQuery.bind(3, item.quantity);
        insertQuery.bind(4, item.price);
        insertQuery.bind(5, item.category);
        insertQuery.bind(6, item.shelf);
        insertQuery.bind(7, item.status);
        insertQuery.bind(8, dateTimeToString(item.createdAt));
        insertQuery.bind(9, dateTimeToString(Domain::now()));
        insertQuery.exec();

        // Remove from removed_items
        SQLite::Statement deleteQuery(*m_itemsDb, "DELETE FROM removed_items WHERE id = ?");
        deleteQuery.bind(1, id);
        deleteQuery.exec();

        transaction.commit();
        return true;
    }
    catch (const std::exception&) {
        return false;
    }
}

// ═══════════════════════════════════════════════════════════════════
// Sale operations
// ═══════════════════════════════════════════════════════════════════

bool SQLiteDataAccess::recordSale(const Domain::Sale& sale)
{
    std::lock_guard<std::mutex> lock(m_mutex);
    if (!m_itemsDb) return false;
    try {
        SQLite::Transaction transaction(*m_itemsDb);
        SQLite::Statement query(*m_itemsDb,
            "INSERT INTO sales (id, itemId, quantitySold, unitPrice, totalAmount, soldBy, saleDate) "
            "VALUES (?, ?, ?, ?, ?, ?, ?)");
        query.bind(1, sale.id);
        query.bind(2, sale.itemId);
        query.bind(3, sale.quantitySold);
        query.bind(4, sale.unitPrice);
        query.bind(5, sale.totalAmount);
        query.bind(6, sale.soldBy);
        query.bind(7, dateTimeToString(sale.saleDate));
        query.exec();
        transaction.commit();
        return true;
    }
    catch (const std::exception&) {
        return false;
    }
}

bool SQLiteDataAccess::deleteSale(const std::string& saleId)
{
    std::lock_guard<std::mutex> lock(m_mutex);
    if (!m_itemsDb) return false;
    try {
        SQLite::Statement query(*m_itemsDb, "DELETE FROM sales WHERE id = ?");
        query.bind(1, saleId);
        query.exec();
        return true;
    }
    catch (const std::exception&) {
        return false;
    }
}

bool SQLiteDataAccess::sellItemAtomic(const std::string& itemId, int quantity, double unitPrice, double totalAmount, const std::string& soldBy)
{
    std::lock_guard<std::mutex> lock(m_mutex);
    if (!m_itemsDb) return false;
    try {
        SQLite::Transaction transaction(*m_itemsDb);

        // 1. Record the sale
        std::string saleId = std::to_string(std::chrono::duration_cast<std::chrono::milliseconds>(
            Domain::now().time_since_epoch()).count());
        SQLite::Statement saleQuery(*m_itemsDb,
            "INSERT INTO sales (id, itemId, quantitySold, unitPrice, totalAmount, soldBy, saleDate) "
            "VALUES (?, ?, ?, ?, ?, ?, ?)");
        saleQuery.bind(1, saleId);
        saleQuery.bind(2, itemId);
        saleQuery.bind(3, quantity);
        saleQuery.bind(4, unitPrice);
        saleQuery.bind(5, totalAmount);
        saleQuery.bind(6, soldBy);
        saleQuery.bind(7, dateTimeToString(Domain::now()));
        saleQuery.exec();

        // 2. Decrement stock atomically
        SQLite::Statement updateQuery(*m_itemsDb,
            "UPDATE items SET quantity = quantity - ?, updatedAt = ? "
            "WHERE id = ? AND deleted = 0 AND quantity >= ?");
        updateQuery.bind(1, quantity);
        updateQuery.bind(2, dateTimeToString(Domain::now()));
        updateQuery.bind(3, itemId);
        updateQuery.bind(4, quantity);
        int rows = updateQuery.exec();

        if (rows == 0) {
            // Insufficient stock or item not found — rollback
            transaction.rollback();
            return false;
        }

        // 3. Auto-update status based on new quantity
        SQLite::Statement qtyQuery(*m_itemsDb,
            "SELECT quantity FROM items WHERE id = ? AND deleted = 0");
        qtyQuery.bind(1, itemId);
        if (qtyQuery.executeStep()) {
            int newQty = qtyQuery.getColumn(0).getInt();
            std::string newStatus;
            if (newQty == 0) newStatus = "Sold Out";
            else if (newQty <= 5) newStatus = "Low Stock";
            else newStatus = "In Stock";

            SQLite::Statement statusQuery(*m_itemsDb,
                "UPDATE items SET status = ? WHERE id = ? AND deleted = 0");
            statusQuery.bind(1, newStatus);
            statusQuery.bind(2, itemId);
            statusQuery.exec();
        }

        transaction.commit();
        return true;
    }
    catch (const std::exception&) {
        return false;
    }
}

std::vector<Domain::Sale> SQLiteDataAccess::getAllSales()
{
    std::lock_guard<std::mutex> lock(m_mutex);
    std::vector<Domain::Sale> sales;
    if (!m_itemsDb) return sales;

    SQLite::Statement query(*m_itemsDb,
        "SELECT id, itemId, quantitySold, unitPrice, totalAmount, soldBy, saleDate "
        "FROM sales ORDER BY saleDate DESC");
    while (query.executeStep()) {
        sales.push_back(rowToSale(query));
    }
    return sales;
}

std::vector<Domain::Sale> SQLiteDataAccess::getSalesForItem(const std::string& itemId)
{
    std::lock_guard<std::mutex> lock(m_mutex);
    std::vector<Domain::Sale> sales;
    if (!m_itemsDb) return sales;

    SQLite::Statement query(*m_itemsDb,
        "SELECT id, itemId, quantitySold, unitPrice, totalAmount, soldBy, saleDate "
        "FROM sales WHERE itemId = ? ORDER BY saleDate DESC");
    query.bind(1, itemId);
    while (query.executeStep()) {
        sales.push_back(rowToSale(query));
    }
    return sales;
}

std::vector<Domain::Sale> SQLiteDataAccess::getSalesByUser(const std::string& userId)
{
    std::lock_guard<std::mutex> lock(m_mutex);
    std::vector<Domain::Sale> sales;
    if (!m_itemsDb) return sales;

    SQLite::Statement query(*m_itemsDb,
        "SELECT id, itemId, quantitySold, unitPrice, totalAmount, soldBy, saleDate "
        "FROM sales WHERE soldBy = ? ORDER BY saleDate DESC");
    query.bind(1, userId);
    while (query.executeStep()) {
        sales.push_back(rowToSale(query));
    }
    return sales;
}

std::vector<Domain::Sale> SQLiteDataAccess::searchSales(const std::string& term, const std::string& field)
{
    std::lock_guard<std::mutex> lock(m_mutex);
    std::vector<Domain::Sale> sales;
    if (!m_itemsDb) return sales;

    std::string sql =
        "SELECT id, itemId, quantitySold, unitPrice, totalAmount, soldBy, saleDate "
        "FROM sales WHERE 1=1";

    if (field == "itemId") {
        sql += " AND itemId LIKE ?";
    } else if (field == "soldBy") {
        sql += " AND soldBy LIKE ?";
    } else if (field == "id") {
        sql += " AND id LIKE ?";
    } else {
        sql += " AND (id LIKE ? OR itemId LIKE ? OR soldBy LIKE ?)";
    }
    sql += " ORDER BY saleDate DESC";

    SQLite::Statement query(*m_itemsDb, sql);
    std::string pattern = "%" + term + "%";

    if (field == "itemId" || field == "soldBy" || field == "id") {
        query.bind(1, pattern);
    } else {
        for (int i = 1; i <= 3; ++i) {
            query.bind(i, pattern);
        }
    }

    while (query.executeStep()) {
        sales.push_back(rowToSale(query));
    }
    return sales;
}

// ═══════════════════════════════════════════════════════════════════
// Category operations
// ═══════════════════════════════════════════════════════════════════

std::vector<Domain::Category> SQLiteDataAccess::getAllCategories()
{
    std::lock_guard<std::mutex> lock(m_mutex);
    std::vector<Domain::Category> categories;
    if (!m_itemsDb) return categories;

    SQLite::Statement query(*m_itemsDb, "SELECT id, name FROM categories ORDER BY name");
    while (query.executeStep()) {
        categories.push_back(rowToCategory(query));
    }
    return categories;
}

bool SQLiteDataAccess::addCategory(const Domain::Category& category)
{
    std::lock_guard<std::mutex> lock(m_mutex);
    if (!m_itemsDb) return false;
    try {
        std::string catId = category.id.empty()
            ? std::to_string(std::chrono::duration_cast<std::chrono::milliseconds>(
                std::chrono::system_clock::now().time_since_epoch()).count())
            : category.id;
        SQLite::Statement query(*m_itemsDb, "INSERT INTO categories (id, name) VALUES (?, ?)");
        query.bind(1, catId);
        query.bind(2, category.name);
        query.exec();
        return true;
    }
    catch (const std::exception&) {
        return false;
    }
}

bool SQLiteDataAccess::updateCategory(const Domain::Category& category)
{
    std::lock_guard<std::mutex> lock(m_mutex);
    if (!m_itemsDb) return false;
    try {
        SQLite::Statement query(*m_itemsDb, "UPDATE categories SET name = ? WHERE id = ?");
        query.bind(1, category.name);
        query.bind(2, category.id);
        int rows = query.exec();
        return rows > 0;
    }
    catch (const std::exception&) {
        return false;
    }
}

bool SQLiteDataAccess::removeCategory(const std::string& id)
{
    std::lock_guard<std::mutex> lock(m_mutex);
    if (!m_itemsDb) return false;
    try {
        SQLite::Statement query(*m_itemsDb, "DELETE FROM categories WHERE id = ?");
        query.bind(1, id);
        int rows = query.exec();
        return rows > 0;
    }
    catch (const std::exception&) {
        return false;
    }
}

// ═══════════════════════════════════════════════════════════════════
// Shelf operations
// ═══════════════════════════════════════════════════════════════════

std::vector<Domain::Shelf> SQLiteDataAccess::getAllShelves()
{
    std::lock_guard<std::mutex> lock(m_mutex);
    std::vector<Domain::Shelf> shelves;
    if (!m_itemsDb) return shelves;

    SQLite::Statement query(*m_itemsDb, "SELECT id, name FROM shelves ORDER BY name");
    while (query.executeStep()) {
        shelves.push_back(rowToShelf(query));
    }
    return shelves;
}

bool SQLiteDataAccess::addShelf(const Domain::Shelf& shelf)
{
    std::lock_guard<std::mutex> lock(m_mutex);
    if (!m_itemsDb) return false;
    try {
        std::string shelfId = shelf.id.empty()
            ? std::to_string(std::chrono::duration_cast<std::chrono::milliseconds>(
                std::chrono::system_clock::now().time_since_epoch()).count())
            : shelf.id;
        SQLite::Statement query(*m_itemsDb, "INSERT INTO shelves (id, name) VALUES (?, ?)");
        query.bind(1, shelfId);
        query.bind(2, shelf.name);
        query.exec();
        return true;
    }
    catch (const std::exception&) {
        return false;
    }
}

bool SQLiteDataAccess::updateShelf(const Domain::Shelf& shelf)
{
    std::lock_guard<std::mutex> lock(m_mutex);
    if (!m_itemsDb) return false;
    try {
        SQLite::Statement query(*m_itemsDb, "UPDATE shelves SET name = ? WHERE id = ?");
        query.bind(1, shelf.name);
        query.bind(2, shelf.id);
        int rows = query.exec();
        return rows > 0;
    }
    catch (const std::exception&) {
        return false;
    }
}

bool SQLiteDataAccess::removeShelf(const std::string& id)
{
    std::lock_guard<std::mutex> lock(m_mutex);
    if (!m_itemsDb) return false;
    try {
        SQLite::Statement query(*m_itemsDb, "DELETE FROM shelves WHERE id = ?");
        query.bind(1, id);
        int rows = query.exec();
        return rows > 0;
    }
    catch (const std::exception&) {
        return false;
    }
}

// ═══════════════════════════════════════════════════════════════════
// User operations
// ═══════════════════════════════════════════════════════════════════

bool SQLiteDataAccess::addUser(const Domain::User& user)
{
    std::lock_guard<std::mutex> lock(m_mutex);
    if (!m_usersDb) return false;
    try {
        qDebug() << "[DB addUser] username:" << QString::fromStdString(user.username) << "id empty:" << user.id.empty();
        SQLite::Statement query(*m_usersDb,
            "INSERT INTO users (id, username, passwordHash, salt, role, createdAt) VALUES (?, ?, ?, ?, ?, ?)");
        query.bind(1, user.id);
        query.bind(2, user.username);
        query.bind(3, user.passwordHash);
        query.bind(4, user.salt);
        query.bind(5, static_cast<int>(user.role));
        query.bind(6, dateTimeToString(user.createdAt));
        query.exec();
        qDebug() << "[DB addUser] SUCCESS";
        return true;
    }
    catch (const std::exception& e) {
        qDebug() << "[DB addUser] FAILED:" << e.what();
        return false;
    }
}

std::optional<Domain::User> SQLiteDataAccess::getUserByUsername(const std::string& username)
{
    std::lock_guard<std::mutex> lock(m_mutex);
    if (!m_usersDb) return std::nullopt;

    SQLite::Statement query(*m_usersDb,
        "SELECT id, username, passwordHash, salt, role, createdAt, lastLogin FROM users WHERE username = ?");
    query.bind(1, username);
    if (query.executeStep()) {
        return rowToUser(query);
    }
    return std::nullopt;
}

bool SQLiteDataAccess::updateUser(const Domain::User& user)
{
    std::lock_guard<std::mutex> lock(m_mutex);
    if (!m_usersDb) return false;
    try {
        SQLite::Statement query(*m_usersDb,
            "UPDATE users SET passwordHash = ?, salt = ?, role = ?, lastLogin = ? WHERE id = ?");
        query.bind(1, user.passwordHash);
        query.bind(2, user.salt);
        query.bind(3, static_cast<int>(user.role));
        query.bind(4, user.lastLogin == Domain::DateTime{} ? "" : dateTimeToString(user.lastLogin));
        query.bind(5, user.id);
        int rows = query.exec();
        return rows > 0;
    }
    catch (const std::exception&) {
        return false;
    }
}

std::vector<Domain::User> SQLiteDataAccess::getAllUsers()
{
    std::lock_guard<std::mutex> lock(m_mutex);
    std::vector<Domain::User> users;
    if (!m_usersDb) return users;

    SQLite::Statement query(*m_usersDb,
        "SELECT id, username, passwordHash, salt, role, createdAt, lastLogin FROM users ORDER BY username");
    while (query.executeStep()) {
        users.push_back(rowToUser(query));
    }
    return users;
}

// ═══════════════════════════════════════════════════════════════════
// ID checking
// ═══════════════════════════════════════════════════════════════════

bool SQLiteDataAccess::checkIdExists(const std::string& entityType, const std::string& id)
{
    std::lock_guard<std::mutex> lock(m_mutex);
    try {
        if (entityType == "items") {
            if (!m_itemsDb) return false;
            SQLite::Statement query(*m_itemsDb, "SELECT 1 FROM items WHERE id = ? AND deleted = 0");
            query.bind(1, id);
            return query.executeStep();
        } else if (entityType == "categories") {
            if (!m_itemsDb) return false;
            SQLite::Statement query(*m_itemsDb, "SELECT 1 FROM categories WHERE id = ?");
            query.bind(1, id);
            return query.executeStep();
        } else if (entityType == "shelves") {
            if (!m_itemsDb) return false;
            SQLite::Statement query(*m_itemsDb, "SELECT 1 FROM shelves WHERE id = ?");
            query.bind(1, id);
            return query.executeStep();
        } else if (entityType == "users") {
            if (!m_usersDb) return false;
            SQLite::Statement query(*m_usersDb, "SELECT 1 FROM users WHERE id = ?");
            query.bind(1, id);
            return query.executeStep();
        } else if (entityType == "sales") {
            if (!m_itemsDb) return false;
            SQLite::Statement query(*m_itemsDb, "SELECT 1 FROM sales WHERE id = ?");
            query.bind(1, id);
            return query.executeStep();
        }
    }
    catch (const std::exception&) {
        return false;
    }
    return false;
}

// ═══════════════════════════════════════════════════════════════════
// Listbox population
// ═══════════════════════════════════════════════════════════════════

std::vector<ListItem> SQLiteDataAccess::populateList(const std::string& entityType, const std::string& searchTerm, const std::string& filterField)
{
    std::vector<ListItem> items;

    try {
        if (entityType == "items") {
            std::vector<Domain::Item> itemList;
            if (searchTerm.empty()) {
                itemList = getAllItems();
            } else {
                itemList = searchItems(searchTerm, filterField);
            }
            for (const auto& item : itemList) {
                items.push_back({item.id, item.toDisplayString(), filterField});
            }
        } else if (entityType == "categories") {
            auto categories = getAllCategories();
            for (const auto& cat : categories) {
                items.push_back({cat.id, cat.name, ""});
            }
        } else if (entityType == "shelves") {
            auto shelves = getAllShelves();
            for (const auto& shelf : shelves) {
                items.push_back({shelf.id, shelf.name, ""});
            }
        } else if (entityType == "users") {
            auto users = getAllUsers();
            for (const auto& user : users) {
                items.push_back({user.id, user.username + " | " + user.roleName(), ""});
            }
        } else if (entityType == "sales") {
            auto sales = getAllSales();
            for (const auto& sale : sales) {
                items.push_back({sale.id, sale.toDisplayString(), ""});
            }
        }
    }
    catch (const std::exception&) {
        // Return empty on error
    }

    return items;
}

} // namespace DataAccess

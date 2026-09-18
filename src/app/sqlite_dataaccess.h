#pragma once

#include "dataaccess.h"
#include <SQLiteCpp/Database.h>
#include <SQLiteCpp/Statement.h>
#include <memory>
#include <mutex>
#include <string>

namespace DataAccess {

class SQLiteDataAccess : public IDataAccess {
public:
    static SQLiteDataAccess& instance() {
        static SQLiteDataAccess db;
        return db;
    }

    SQLiteDataAccess();
    ~SQLiteDataAccess() override;

    void initialize(const std::string& itemsDb, const std::string& usersDb) override;
    void shutdown() override;
    bool isConnected() const override;

    // Item operations
    std::vector<Domain::Item> getAllItems() override;
    std::optional<Domain::Item> getItemById(const std::string& id) override;
    std::vector<Domain::Item> searchItems(const std::string& term, const std::string& field) override;
    bool addItem(const Domain::Item& item) override;
    bool updateItem(const Domain::Item& item) override;
    bool removeItem(const std::string& id) override;
    std::vector<Domain::Item> getRemovedItems() override;
    bool restoreItem(const std::string& id) override;

    // Sale operations
    bool recordSale(const Domain::Sale& sale) override;
    bool deleteSale(const std::string& saleId) override;
    bool sellItemAtomic(const std::string& itemId, int quantity, double unitPrice, double totalAmount, const std::string& soldBy) override;
    std::vector<Domain::Sale> getAllSales() override;
    std::vector<Domain::Sale> getSalesForItem(const std::string& itemId) override;
    std::vector<Domain::Sale> getSalesByUser(const std::string& userId) override;
    std::vector<Domain::Sale> searchSales(const std::string& term, const std::string& field) override;

    // Daily statistics snapshots
    bool upsertDailyStat(const Domain::DailyStat& stat) override;
    std::vector<Domain::DailyStat> getDailyStats() override;

    // Category operations
    std::vector<Domain::Category> getAllCategories() override;
    bool addCategory(const Domain::Category& category) override;
    bool updateCategory(const Domain::Category& category) override;
    bool removeCategory(const std::string& id) override;

    // Shelf operations
    std::vector<Domain::Shelf> getAllShelves() override;
    bool addShelf(const Domain::Shelf& shelf) override;
    bool updateShelf(const Domain::Shelf& shelf) override;
    bool removeShelf(const std::string& id) override;

    // User operations
    bool addUser(const Domain::User& user) override;
    std::optional<Domain::User> getUserByUsername(const std::string& username) override;
    bool updateUser(const Domain::User& user) override;
    std::vector<Domain::User> getAllUsers() override;

    // ID checking
    bool checkIdExists(const std::string& entityType, const std::string& id) override;

    // Listbox population
    std::vector<ListItem> populateList(const std::string& entityType, const std::string& searchTerm = "", const std::string& filterField = "") override;

private:
    void createTables();
    void createIndexes();
    Domain::Item rowToItem(const SQLite::Statement& stmt);
    Domain::Sale rowToSale(const SQLite::Statement& stmt);
    Domain::Category rowToCategory(const SQLite::Statement& stmt);
    Domain::Shelf rowToShelf(const SQLite::Statement& stmt);
    Domain::User rowToUser(const SQLite::Statement& stmt);
    Domain::DailyStat rowToDailyStat(const SQLite::Statement& stmt);
    Domain::DailyStat dailyStatFor(const Domain::Sale& sale);
    void upsertDailyStatLocked(const Domain::DailyStat& stat); // caller holds m_mutex
    void rebuildDailyStatLocked(const std::string& day);       // caller holds m_mutex
    std::string dateTimeToString(const Domain::DateTime& dt);
    Domain::DateTime stringToDateTime(const std::string& str);

    std::unique_ptr<SQLite::Database> m_itemsDb;
    std::unique_ptr<SQLite::Database> m_usersDb;
    mutable std::mutex m_mutex;
    bool m_connected = false;
};

} // namespace DataAccess

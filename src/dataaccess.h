#pragma once

#include "domain.h"
#include <string>
#include <optional>
#include <vector>
#include <exception>

namespace DataAccess {

struct ListItem {
    std::string id;
    std::string displayText;
    std::string searchField;
};

class DataAccessException : public std::exception {
public:
    explicit DataAccessException(const std::string& message) : m_message(message) {}
    const char* what() const noexcept override { return m_message.c_str(); }
    const std::string& message() const { return m_message; }
private:
    std::string m_message;
};

class IDataAccess {
public:
    virtual ~IDataAccess() = default;
    virtual void initialize(const std::string& itemsDb, const std::string& usersDb) = 0;
    virtual void shutdown() = 0;
    virtual bool isConnected() const = 0;

    // Item operations
    virtual std::vector<Domain::Item> getAllItems() = 0;
    virtual std::optional<Domain::Item> getItemById(const std::string& id) = 0;
    virtual std::vector<Domain::Item> searchItems(const std::string& term, const std::string& field) = 0;
    virtual bool addItem(const Domain::Item& item) = 0;
    virtual bool updateItem(const Domain::Item& item) = 0;
    virtual bool removeItem(const std::string& id) = 0;
    virtual std::vector<Domain::Item> getRemovedItems() = 0;
    virtual bool restoreItem(const std::string& id) = 0;

    // Sale operations
    virtual bool recordSale(const Domain::Sale& sale) = 0;
    virtual bool deleteSale(const std::string& saleId) = 0;
    virtual bool sellItemAtomic(const std::string& itemId, int quantity, double unitPrice, double totalAmount, const std::string& soldBy) = 0;
    virtual std::vector<Domain::Sale> getAllSales() = 0;
    virtual std::vector<Domain::Sale> getSalesForItem(const std::string& itemId) = 0;
    virtual std::vector<Domain::Sale> getSalesByUser(const std::string& userId) = 0;
    virtual std::vector<Domain::Sale> searchSales(const std::string& term, const std::string& field) = 0;

    // Category operations
    virtual std::vector<Domain::Category> getAllCategories() = 0;
    virtual bool addCategory(const Domain::Category& category) = 0;
    virtual bool updateCategory(const Domain::Category& category) = 0;
    virtual bool removeCategory(const std::string& id) = 0;

    // Shelf operations
    virtual std::vector<Domain::Shelf> getAllShelves() = 0;
    virtual bool addShelf(const Domain::Shelf& shelf) = 0;
    virtual bool updateShelf(const Domain::Shelf& shelf) = 0;
    virtual bool removeShelf(const std::string& id) = 0;

    // User operations
    virtual bool addUser(const Domain::User& user) = 0;
    virtual std::optional<Domain::User> getUserByUsername(const std::string& username) = 0;
    virtual bool updateUser(const Domain::User& user) = 0;
    virtual std::vector<Domain::User> getAllUsers() = 0;

    // ID checking
    virtual bool checkIdExists(const std::string& entityType, const std::string& id) = 0;

    // Listbox population
    virtual std::vector<ListItem> populateList(const std::string& entityType, const std::string& searchTerm = "", const std::string& filterField = "") = 0;
};

} // namespace DataAccess

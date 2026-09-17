#pragma once

#include "domain.h"
#include "dataaccess.h"
#include <string>
#include <optional>
#include <vector>

namespace BusinessLogic {

struct ValidationResult {
    bool isValid = false;
    std::string errorMessage;

    static ValidationResult success() {
        return ValidationResult{true, ""};
    }

    static ValidationResult failure(const std::string& message) {
        return ValidationResult{false, message};
    }
};

struct DatabaseValidationResult {
    bool isValid = false;
    std::string errorMessage;

    static DatabaseValidationResult success() {
        return DatabaseValidationResult{true, ""};
    }

    static DatabaseValidationResult failure(const std::string& message) {
        return DatabaseValidationResult{false, message};
    }
};

DatabaseValidationResult validateDatabases(DataAccess::IDataAccess& db);

// Role-based access control
enum class RequiredRole { None = 0, Admin = 2, SuperAdmin = 3 };

struct RoleCheckResult {
    bool hasAccess = false;
    std::string errorMessage;
    static RoleCheckResult success() { return RoleCheckResult{true, ""}; }
    static RoleCheckResult failure(const std::string& msg) { return RoleCheckResult{false, msg}; }
};

RoleCheckResult checkUserRole(const std::optional<Domain::User>& currentUser, RequiredRole required);
RoleCheckResult checkUserRole(int userRoleInt, RequiredRole required);

// Validation functions
ValidationResult validateItemDTO(const DTO::ItemDTO& item);
ValidationResult validateSaleDTO(const DTO::SaleDTO& sale);
ValidationResult validateCategoryDTO(const DTO::CategoryDTO& category);
ValidationResult validateShelfDTO(const DTO::ShelfDTO& shelf);
ValidationResult validateUserDTO(const DTO::UserDTO& user);

// CRUD functions: validate then call data access
ValidationResult addItem(DataAccess::IDataAccess& db, const DTO::ItemDTO& item);
ValidationResult addCategory(DataAccess::IDataAccess& db, const DTO::CategoryDTO& category);
ValidationResult addShelf(DataAccess::IDataAccess& db, const DTO::ShelfDTO& shelf);
ValidationResult addUser(DataAccess::IDataAccess& db, const DTO::UserDTO& user);

ValidationResult updateItem(DataAccess::IDataAccess& db, const DTO::ItemDTO& item);
ValidationResult updateCategory(DataAccess::IDataAccess& db, const DTO::CategoryDTO& category);
ValidationResult updateShelf(DataAccess::IDataAccess& db, const DTO::ShelfDTO& shelf);

// Sale logic
ValidationResult sellItem(DataAccess::IDataAccess& db, const std::string& itemId, int quantity, const std::string& clerkId);

// Authentication
std::optional<Domain::User> login(DataAccess::IDataAccess& db, const std::string& username, const std::string& password);

// Crypto initialization
bool initializeCrypto();

// Database lifecycle
bool initializeDatabases(DataAccess::IDataAccess& db, const std::string& itemsDb, const std::string& usersDb);
void shutdownDatabases(DataAccess::IDataAccess& db);
bool isDatabaseConnected(DataAccess::IDataAccess& db);

// List population
std::vector<DataAccess::ListItem> populateList(DataAccess::IDataAccess& db, const std::string& entityType, const std::string& searchTerm = "", const std::string& filterField = "");

} // namespace BusinessLogic

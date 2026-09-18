#include "businesslogic.h"
#include "domain.h"
#include "crypto.h"
#include <algorithm>
#include <cctype>
#include <QDebug>

namespace BusinessLogic {

static std::string trim(const std::string& str) {
    size_t first = str.find_first_not_of(" \t\n\r");
    if (first == std::string::npos) return "";
    size_t last = str.find_last_not_of(" \t\n\r");
    return str.substr(first, last - first + 1);
}

// ============ Validation ============

ValidationResult validateItemDTO(const DTO::ItemDTO& item) {
    if (trim(item.name).empty()) {
        return ValidationResult::failure("Item name cannot be empty");
    }
    if (!Domain::isValidId(item.id)) {
        return ValidationResult::failure("Item ID cannot be empty");
    }
    if (item.quantity < 0) {
        return ValidationResult::failure("Quantity cannot be negative");
    }
    if (item.price <= 0.0) {
        return ValidationResult::failure("Price must be greater than zero");
    }
    if (trim(item.status).empty()) {
        return ValidationResult::failure("Item status cannot be empty");
    }
    return ValidationResult::success();
}

ValidationResult validateSaleDTO(const DTO::SaleDTO& sale) {
    if (!Domain::isValidId(sale.id)) {
        return ValidationResult::failure("Sale ID cannot be empty");
    }
    if (!Domain::isValidId(sale.itemId)) {
        return ValidationResult::failure("Sale item ID cannot be empty");
    }
    if (sale.quantitySold <= 0) {
        return ValidationResult::failure("Quantity sold must be at least 1");
    }
    return ValidationResult::success();
}

ValidationResult validateCategoryDTO(const DTO::CategoryDTO& category) {
    if (trim(category.name).empty()) {
        return ValidationResult::failure("Category name cannot be empty");
    }
    if (!Domain::isValidId(category.id)) {
        return ValidationResult::failure("Category ID cannot be empty");
    }
    return ValidationResult::success();
}

ValidationResult validateShelfDTO(const DTO::ShelfDTO& shelf) {
    if (trim(shelf.name).empty()) {
        return ValidationResult::failure("Shelf name cannot be empty");
    }
    if (!Domain::isValidId(shelf.id)) {
        return ValidationResult::failure("Shelf ID cannot be empty");
    }
    return ValidationResult::success();
}

ValidationResult validateUserDTO(const DTO::UserDTO& user) {
    if (trim(user.username).empty()) {
        return ValidationResult::failure("Username cannot be empty");
    }
    if (trim(user.password).empty()) {
        return ValidationResult::failure("Password cannot be empty");
    }
    if (user.username.length() < 3) {
        return ValidationResult::failure("Username must be at least 3 characters");
    }
    if (user.password.length() < 8) {
        return ValidationResult::failure("Password must be at least 8 characters");
    }
    if (user.username == user.password) {
        return ValidationResult::failure("Username and password cannot be the same");
    }
    if (!user.role.has_value()) {
        return ValidationResult::failure("Role must be selected");
    }
    return ValidationResult::success();
}

// ============ CRUD: validate then persist ============

ValidationResult addItem(DataAccess::IDataAccess& db, const DTO::ItemDTO& item) {
    auto validation = validateItemDTO(item);
    if (!validation.isValid) {
        return validation;
    }
    Domain::Item domainItem = item.toDomain();
    if (db.addItem(domainItem)) {
        return ValidationResult::success();
    }
    return ValidationResult::failure("Failed to add item to database");
}

ValidationResult addCategory(DataAccess::IDataAccess& db, const DTO::CategoryDTO& category) {
    auto validation = validateCategoryDTO(category);
    if (!validation.isValid) {
        return validation;
    }
    Domain::Category domainCategory = category.toDomain();
    if (db.addCategory(domainCategory)) {
        return ValidationResult::success();
    }
    return ValidationResult::failure("Failed to add category to database");
}

ValidationResult addShelf(DataAccess::IDataAccess& db, const DTO::ShelfDTO& shelf) {
    auto validation = validateShelfDTO(shelf);
    if (!validation.isValid) {
        return validation;
    }
    Domain::Shelf domainShelf = shelf.toDomain();
    if (db.addShelf(domainShelf)) {
        return ValidationResult::success();
    }
    return ValidationResult::failure("Failed to add shelf to database");
}

ValidationResult addUser(DataAccess::IDataAccess& db, const DTO::UserDTO& user) {
    qDebug() << "[BL addUser] validating...";
    auto validation = validateUserDTO(user);
    if (!validation.isValid) {
        qDebug() << "[BL addUser] validation FAILED:" << QString::fromStdString(validation.errorMessage);
        return validation;
    }
    qDebug() << "[BL addUser] validation OK, hashing password...";
    // Hash the password before storing
    std::string hashedPwd = hash_string(user.password);
    if (hashedPwd.empty()) {
        return ValidationResult::failure("Failed to hash password (out of memory?)");
    }
    DTO::UserDTO hashedUser = user;
    hashedUser.password = hashedPwd;
    qDebug() << "[BL addUser] converting to domain...";
    Domain::User domainUser = hashedUser.toDomain();
    qDebug() << "[BL addUser] calling db.addUser...";
    if (db.addUser(domainUser)) {
        qDebug() << "[BL addUser] SUCCESS";
        return ValidationResult::success();
    }
    qDebug() << "[BL addUser] db.addUser FAILED";
    return ValidationResult::failure("Failed to register user");
}

ValidationResult updateItem(DataAccess::IDataAccess& db, const DTO::ItemDTO& item) {
    auto validation = validateItemDTO(item);
    if (!validation.isValid) {
        return validation;
    }
    Domain::Item domainItem = item.toDomain();
    if (db.updateItem(domainItem)) {
        return ValidationResult::success();
    }
    return ValidationResult::failure("Failed to update item in database");
}

ValidationResult updateCategory(DataAccess::IDataAccess& db, const DTO::CategoryDTO& category) {
    auto validation = validateCategoryDTO(category);
    if (!validation.isValid) {
        return validation;
    }
    Domain::Category domainCategory = category.toDomain();
    if (db.updateCategory(domainCategory)) {
        return ValidationResult::success();
    }
    return ValidationResult::failure("Failed to update category in database");
}

ValidationResult updateShelf(DataAccess::IDataAccess& db, const DTO::ShelfDTO& shelf) {
    auto validation = validateShelfDTO(shelf);
    if (!validation.isValid) {
        return validation;
    }
    Domain::Shelf domainShelf = shelf.toDomain();
    if (db.updateShelf(domainShelf)) {
        return ValidationResult::success();
    }
    return ValidationResult::failure("Failed to update shelf in database");
}

// ============ Sale Logic ============

ValidationResult sellItem(DataAccess::IDataAccess& db, const std::string& itemId, int quantity, const std::string& clerkId) {
    if (quantity <= 0) {
        return ValidationResult::failure("Quantity must be at least 1");
    }

    auto itemOpt = db.getItemById(itemId);
    if (!itemOpt.has_value()) {
        return ValidationResult::failure("Item not found");
    }

    Domain::Item item = itemOpt.value();
    if (item.quantity < quantity) {
        return ValidationResult::failure("Insufficient stock (available: " + std::to_string(item.quantity) + ")");
    }

    double totalAmount = item.price * quantity;

    // Use atomic method — sale + stock decrement in a single transaction
    if (!db.sellItemAtomic(itemId, quantity, item.price, totalAmount, clerkId)) {
        return ValidationResult::failure("Failed to complete sale (insufficient stock or DB error)");
    }

    return ValidationResult::success();
}

// ============ Database ============

DatabaseValidationResult validateDatabases(DataAccess::IDataAccess& db) {
    if (!db.isConnected()) {
        return DatabaseValidationResult::failure("No database connection established");
    }
    return DatabaseValidationResult::success();
}

// ============ Role check ============

RoleCheckResult checkUserRole(const std::optional<Domain::User>& currentUser, RequiredRole required) {
    if (!currentUser.has_value()) {
        return RoleCheckResult::failure("No user logged in");
    }
    int userRole = static_cast<int>(currentUser->role);
    return checkUserRole(userRole, required);
}

RoleCheckResult checkUserRole(int userRoleInt, RequiredRole required) {
    int requiredRole = static_cast<int>(required);
    if (userRoleInt < requiredRole) {
        std::string roleName;
        switch (required) {
            case RequiredRole::Admin: roleName = "Admin"; break;
            case RequiredRole::SuperAdmin: roleName = "SuperAdmin"; break;
            default: roleName = "Unknown"; break;
        }
        return RoleCheckResult::failure("Access denied. Requires " + roleName + " role or higher.");
    }
    return RoleCheckResult::success();
}

// ============ Authentication ============

std::optional<Domain::User> login(DataAccess::IDataAccess& db, const std::string& username, const std::string& password) {
    qDebug() << "[BL login] looking up user:" << QString::fromStdString(username);
    auto userOpt = db.getUserByUsername(username);
    if (!userOpt.has_value()) {
        qDebug() << "[BL login] user not found";
        return std::nullopt;
    }
    qDebug() << "[BL login] user found, role:" << static_cast<int>(userOpt->role) << "comparing passwords...";
    // Verify the Argon2id hash stored in the database
    bool match = verify_string(userOpt->passwordHash, password);
    if (!match) {
        qDebug() << "[BL login] password mismatch - FAILED";
        return std::nullopt;
    }
    qDebug() << "[BL login] password match - SUCCESS";
    return userOpt;
}

bool initializeCrypto() {
    return load_libsodium();
}

bool initializeDatabases(DataAccess::IDataAccess& db, const std::string& itemsDb, const std::string& usersDb) {
    try {
        db.initialize(itemsDb, usersDb);
        return true;
    } catch (const std::exception& e) {
        qDebug() << "[BL initializeDatabases] FAILED:" << e.what();
        return false;
    }
}

void shutdownDatabases(DataAccess::IDataAccess& db) {
    db.shutdown();
}

bool isDatabaseConnected(DataAccess::IDataAccess& db) {
    return db.isConnected();
}

std::vector<DataAccess::ListItem> populateList(DataAccess::IDataAccess& db, const std::string& entityType, const std::string& searchTerm, const std::string& filterField) {
    return db.populateList(entityType, searchTerm, filterField);
}

} // namespace BusinessLogic

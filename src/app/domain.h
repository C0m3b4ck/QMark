#pragma once

#include <string>
#include <chrono>
#include <vector>
#include <optional>
#include <sstream>
#include <iomanip>

namespace Domain {

// ── Currency / price formatting ─────────────────────────────────────
// The default currency is PLN (polski złoty). The symbol is a plain
// global so the (header-only) domain layer can format prices without
// depending on QSettings. The app calls setCurrencySymbol() from the
// settings (SuperAdmin-only currency switch in Preferences).
inline std::string& currencySymbol() {
    static std::string sym = "zł";
    return sym;
}

inline void setCurrencySymbol(const std::string& sym) {
    currencySymbol() = sym;
}

using DateTime = std::chrono::system_clock::time_point;

inline DateTime now() {
    return std::chrono::system_clock::now();
}

inline std::string toISOString(const DateTime& dt) {
    auto t = std::chrono::system_clock::to_time_t(dt);
    std::stringstream ss;
    ss << std::put_time(std::gmtime(&t), "%Y-%m-%dT%H:%M:%SZ");
    return ss.str();
}

inline DateTime fromISOString(const std::string& str) {
    std::tm tm = {};
    std::istringstream ss(str);
    ss >> std::get_time(&tm, "%Y-%m-%dT%H:%M:%SZ");
    if (ss.fail()) {
        return DateTime{};
    }
    return std::chrono::system_clock::from_time_t(std::mktime(&tm));
}

inline bool isNull(const DateTime& dt) {
    return dt == DateTime{};
}

inline bool isValidId(const std::string& id) {
    return !id.empty() && id != "0";
}

// ── Item (replaces Book) ──────────────────────────────────────────

struct Item {
    std::string id;
    std::string name;
    int quantity = 0;
    double price = 0.0;
    std::string category;
    std::string shelf;          // physical shelf / location in shop
    std::string status;         // "In Stock", "Low Stock", "Out of Stock", "Sold Out"
    DateTime createdAt;
    DateTime updatedAt;

    bool isValid() const {
        return !name.empty() && isValidId(id) && quantity >= 0 && price >= 0.0;
    }

    std::string toDisplayString() const {
        return name + " | Qty: " + std::to_string(quantity) +
               " | " + formatPrice(price) +
               " | " + status +
               " | Shelf: " + shelf +
               " | Cat: " + category +
               " | ID: " + id;
    }

    static std::string formatPrice(double p) {
        std::ostringstream oss;
        oss << std::fixed << std::setprecision(2) << p;
        std::string num = oss.str();
        if (currencySymbol() == "zł") {
            // Polish convention: comma decimal separator, symbol after the number
            for (auto& c : num) {
                if (c == '.') c = ',';
            }
            return num + " zł";
        }
        return "$" + num;
    }
};

// ── Sale (replaces Loan) ──────────────────────────────────────────

struct Sale {
    std::string id;
    std::string itemId;
    int quantitySold = 0;
    double unitPrice = 0.0;
    double totalAmount = 0.0;
    std::string soldBy;         // clerk user ID
    DateTime saleDate;

    std::string toDisplayString() const {
        return "Sale " + id + " | Item: " + itemId +
               " | Qty: " + std::to_string(quantitySold) +
               " | " + Item::formatPrice(totalAmount) +
               " | By: " + soldBy +
               " | " + Domain::toISOString(saleDate);
    }
};

// ── Category ──────────────────────────────────────────────────────

struct Category {
    std::string id;
    std::string name;

    bool isValid() const {
        return !name.empty() && isValidId(id);
    }
};

// ── Shelf (replaces Location) ─────────────────────────────────────

struct Shelf {
    std::string id;
    std::string name;

    bool isValid() const {
        return !name.empty() && isValidId(id);
    }
};

// ── User (unchanged role system) ──────────────────────────────────

struct User {
    enum class Role { UserRole = 1, Admin = 2, SuperAdmin = 3 };

    std::string id;
    std::string username;
    std::string passwordHash;
    std::string salt;
    Role role = Role::UserRole;
    DateTime createdAt;
    DateTime lastLogin;

    bool isValid() const {
        return !username.empty() && !passwordHash.empty() && isValidId(id);
    }

    std::string roleName() const {
        switch (role) {
            case Role::UserRole:    return "Clerk";
            case Role::Admin:       return "Admin";
            case Role::SuperAdmin:  return "SuperAdmin";
        }
        return "Unknown";
    }
};

// ── Undo Entry ────────────────────────────────────────────────────

struct UndoEntry {
    enum class Type { Add, Edit, Remove };
    enum class Entity { ItemEntity, SaleEntity, CategoryEntity, ShelfEntity };

    Type type;
    Entity entity;
    std::string previousData;
    std::string currentData;
    DateTime timestamp;
};

} // namespace Domain

// ── DTOs ──────────────────────────────────────────────────────────

namespace DTO {

struct ItemDTO {
    std::string id;
    std::string name;
    int quantity = 0;
    double price = 0.0;
    std::string category;
    std::string shelf;
    std::string status;
    std::string createdAt;
    std::string updatedAt;

    bool isValid() const {
        return !name.empty() && Domain::isValidId(id);
    }

    std::string toDisplayString() const {
        return name + " | Qty: " + std::to_string(quantity) +
               " | " + Domain::Item::formatPrice(price) +
               " | " + status +
               " | Shelf: " + shelf +
               " | Cat: " + category +
               " | ID: " + id;
    }

    Domain::Item toDomain() const {
        Domain::Item item;
        item.id = id;
        item.name = name;
        item.quantity = quantity;
        item.price = price;
        item.category = category;
        item.shelf = shelf;
        item.status = status;
        item.createdAt = Domain::fromISOString(createdAt);
        item.updatedAt = Domain::fromISOString(updatedAt);
        return item;
    }

    static ItemDTO fromDomain(const Domain::Item& item) {
        ItemDTO dto;
        dto.id = item.id;
        dto.name = item.name;
        dto.quantity = item.quantity;
        dto.price = item.price;
        dto.category = item.category;
        dto.shelf = item.shelf;
        dto.status = item.status;
        dto.createdAt = Domain::toISOString(item.createdAt);
        dto.updatedAt = Domain::toISOString(item.updatedAt);
        return dto;
    }
};

struct SaleDTO {
    std::string id;
    std::string itemId;
    int quantitySold = 0;
    double unitPrice = 0.0;
    double totalAmount = 0.0;
    std::string soldBy;
    std::string saleDate;

    bool isValid() const {
        return Domain::isValidId(id) && Domain::isValidId(itemId) && quantitySold > 0;
    }

    Domain::Sale toDomain() const {
        Domain::Sale sale;
        sale.id = id;
        sale.itemId = itemId;
        sale.quantitySold = quantitySold;
        sale.unitPrice = unitPrice;
        sale.totalAmount = totalAmount;
        sale.soldBy = soldBy;
        sale.saleDate = Domain::fromISOString(saleDate);
        return sale;
    }

    static SaleDTO fromDomain(const Domain::Sale& sale) {
        SaleDTO dto;
        dto.id = sale.id;
        dto.itemId = sale.itemId;
        dto.quantitySold = sale.quantitySold;
        dto.unitPrice = sale.unitPrice;
        dto.totalAmount = sale.totalAmount;
        dto.soldBy = sale.soldBy;
        dto.saleDate = Domain::toISOString(sale.saleDate);
        return dto;
    }
};

struct CategoryDTO {
    std::string id;
    std::string name;

    bool isValid() const {
        return !name.empty() && Domain::isValidId(id);
    }

    Domain::Category toDomain() const {
        Domain::Category cat;
        cat.id = id;
        cat.name = name;
        return cat;
    }

    static CategoryDTO fromDomain(const Domain::Category& cat) {
        CategoryDTO dto;
        dto.id = cat.id;
        dto.name = cat.name;
        return dto;
    }
};

struct ShelfDTO {
    std::string id;
    std::string name;

    bool isValid() const {
        return !name.empty() && Domain::isValidId(id);
    }

    Domain::Shelf toDomain() const {
        Domain::Shelf shelf;
        shelf.id = id;
        shelf.name = name;
        return shelf;
    }

    static ShelfDTO fromDomain(const Domain::Shelf& shelf) {
        ShelfDTO dto;
        dto.id = shelf.id;
        dto.name = shelf.name;
        return dto;
    }
};

struct UserDTO {
    std::string id;
    std::string username;
    std::string password;
    std::optional<Domain::User::Role> role;

    bool isValid() const {
        return !username.empty() && !password.empty() && Domain::isValidId(id);
    }

    // toDomain() stores the RAW password; caller is responsible for hashing
    // before passing to addUser() or updateUser().
    Domain::User toDomain() const {
        Domain::User user;
        user.id = id.empty() ? std::to_string(std::chrono::duration_cast<std::chrono::milliseconds>(
            std::chrono::system_clock::now().time_since_epoch()).count()) : id;
        user.username = username;
        user.passwordHash = password;   // expected to be already hashed
        user.role = role.value_or(Domain::User::Role::UserRole);
        user.createdAt = Domain::now();
        return user;
    }
};

} // namespace DTO

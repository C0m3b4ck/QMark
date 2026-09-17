# QMark — School Shop Point of Sale

> A free, open-source, cross-platform PoS system for school shops.  
> Built on the [ShelfSight](https://github.com/C0m3b4ck/ShelfSight) library management architecture.

---

## About

QMark runs on **Windows** (2000 through 11), **Linux**, and tablets.  
It provides a touch-friendly interface for clerks to browse items, sell them, and track inventory — all from a responsive window that fits any screen size.

All builds are **completely statically linked** — no external DLLs or shared libraries are required. Just copy the executable and go.

---

## Features

| Feature | Description |
|---|---|
| **POS Grid View** | Touch-friendly item cards with large SELL buttons — auto-adapts columns to screen width |
| **Item Management** | Add, edit, search, soft-delete items with name, quantity, price, category, shelf |
| **Atomic Sell** | Sale + stock decrement + auto-status update in a single DB transaction |
| **Sales History** | Complete transaction log with search, filtering, and CSV export |
| **Sales Reports** | Revenue summaries, stock levels, low-stock alerts |
| **Category Management** | Organize items by custom categories |
| **Shelf Management** | Track physical shelf locations in the shop |
| **User Authentication** | Role-based access: Clerk, Admin, SuperAdmin |
| **Password Security** | Argon2id hashing via libsodium with per-user salts |
| **Undo / Restore** | Soft-deletion with restore for items |
| **Worklog** | Session-based change tracking with timestamped entries |
| **Telemetry** | Optional dual-sink logging (CSV file + SQLite DB) |
| **Database Config** | Connect to multiple SQLite databases, save/load configs |
| **Troubleshooting** | Built-in DB test, log viewer, and diagnostic export |

---

## Roles

| Role | Permissions |
|---|---|
| **Clerk** | Sell items, browse inventory |
| **Admin** | Add/edit/remove items, categories, shelves; view reports and sales |
| **SuperAdmin** | All Admin permissions + manage user accounts and roles |

---

## Platform Support

| Platform | Architecture | Minimum OS | Notes |
|---|---|---|---|
| **Windows** | x64 | Windows 2000+ | Statically linked, no DLLs needed |
| **Windows** | x86 | Windows 2000+ | Statically linked, no DLLs needed |
| **Linux** | x64 | glibc 2.31+ (Ubuntu 20.04+) | Statically linked where possible |
| **Linux** | x86 | glibc 2.31+ | Statically linked where possible |

---

## Quick Start

1. Download the appropriate binary for your platform from [Releases](https://github.com/user/QMark/releases)
2. Place the executable in any folder
3. Run `QMark` (or `QMark.exe` on Windows)
4. On first launch, databases (`items.db` and `users.db`) are created automatically in the current directory
5. Go to **Tools → Database Selection** to configure paths if needed
6. Register a SuperAdmin account (see First-Time Setup below)
7. Log in and start selling

---

## First-Time Setup

On first launch, QMark detects that no user accounts exist and automatically prompts you to create a **SuperAdmin** account through a guided setup wizard. Simply enter a username and password when prompted.

After the initial SuperAdmin is created, you can log in and create additional user accounts from the **Accounts** page.

---

## Building from Source

See **[BUILD.md](BUILD.md)** for complete build instructions for all supported platforms:

- **Linux x64** — native build
- **Linux x86** — cross-compile from x64
- **Windows x64** — cross-compile from Linux or native MSYS2 build
- **Windows x86** — cross-compile from Linux or native MSYS2 build

All builds are completely statically linked.

---

## Project Layout

```
QMark/
├── src/                          # Application source
│   ├── main.cpp                  # Entry point
│   ├── domain.h                  # Data models (Item, Sale, User, Category, Shelf) + DTOs
│   ├── dataaccess.h              # Abstract data access interface (IDataAccess)
│   ├── sqlite_dataaccess.h/cpp   # SQLite persistence layer (950 LOC)
│   ├── businesslogic.h/cpp       # Validation, CRUD orchestration, sale logic
│   ├── crypto.h/cpp              # Argon2id password hashing (libsodium)
│   ├── mainwindow.h/cpp/ui       # Qt6 interface with POS grid (1300+ LOC)
│   ├── logger.h                  # Singleton logger with file output
│   ├── telemetry.h               # Dual-sink telemetry (CSV + SQLite)
│   ├── worklog.h                 # Session-based change tracker
│   ├── sanitize_string.cpp       # Input sanitization for SQL safety
│   ├── QMark.pro                 # Top-level qmake project (SUBDIRS)
│   ├── app.pro                   # Application qmake project
│   ├── mingw64.cmake             # CMake toolchain for Windows cross-compilation
│   └── sqlitecpp/                # SQLiteCpp static library (qmake project)
├── sqlitecpp/                    # Vendored SQLiteCpp library (source)
│   ├── include/SQLiteCpp/        # Public headers
│   ├── src/                      # Library source
│   └── sqlite3/                  # Bundled SQLite3 amalgamation
├── BUILD.md                      # Build instructions for all platforms
└── README.md                     # This file
```

---

## Architecture

QMark uses a **layered architecture** separating concerns:

```
┌─────────────────────────────────────────────┐
│  GUI Layer  (mainwindow.h/cpp/ui)           │  Qt 6 widgets, POS grid, 17 pages
├─────────────────────────────────────────────┤
│  Business Logic  (businesslogic.h/cpp)      │  Validation, CRUD, sell, auth
├─────────────────────────────────────────────┤
│  Data Access  (sqlite_dataaccess.h/cpp)     │  SQLite persistence, thread-safe
├─────────────────────────────────────────────┤
│  SQLiteCpp + SQLite3  (vendored)            │  Database engine
└─────────────────────────────────────────────┘
```

Each entity has its own SQLite table:

| Table | Purpose |
|---|---|
| `items` | Active inventory items |
| `removed_items` | Soft-deleted items (for undo) |
| `sales` | Transaction history |
| `categories` | Item categories |
| `shelves` | Physical shelf locations |
| `users` | User accounts with hashed passwords |

---

## Dependencies

| Library | Purpose | Linkage |
|---|---|---|
| **Qt 6** | GUI framework (Core, Gui, Widgets, Sql) | Static |
| **SQLite3** | Embedded database engine | Static (vendored amalgamation) |
| **SQLiteCpp** | C++ wrapper around SQLite3 | Static (vendored source) |
| **libsodium** | Argon2id password hashing | Static |

---

## Security

- Passwords hashed with **Argon2id** via libsodium with per-user salts
- Input sanitization prevents SQL injection
- Role-based access control restricts operations by user role
- Role changes restricted to **SuperAdmin** accounts only

---

## Screen Sizes

The window opens **maximized** — fitting to whatever screen is available. The POS grid auto-calculates how many item cards fit per row based on window width (~240px per card + margins), so it works on:

- 1080p desktop monitors
- 720p tablets in landscape
- Any resolution down to 800×600

---

## ShelfSight Feature Mapping

| ShelfSight (Library) | QMark (Shop PoS) |
|---|---|
| Book | **Item** (name, quantity, price) |
| Reader | *(removed — clerk roles instead)* |
| Loan | **Sale** (transaction) |
| Category | **Category** |
| Location | **Shelf** |
| Loan System | **Sell Item (mark as sold)** |
| Reports | **Sales Reports** |
| User Roles | **Clerk / Admin / SuperAdmin** |

---

## License

Apache 2.0 — same as ShelfSight.

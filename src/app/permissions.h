#pragma once

#include "domain.h"
#include "dataaccess.h"
#include <map>
#include <string>
#include <vector>

// ── Feature permissions ───────────────────────────────────────────
// A small, fixed registry of gated features plus the logic that resolves
// whether a given user currently has one of them.
//
// Resolution order (lowest to highest precedence):
//   1. the role's built-in default (reproduces the pre-existing behaviour);
//   2. a template assigned to the user's role;
//   3. a template assigned to the user directly;
//   4. a direct grant on the user's role;
//   5. a direct grant on the user (overrides everything).
// Direct grants can both allow (enabled) and deny (disabled) a feature.
namespace Permissions {

enum class Feature {
    Resupply,
    ItemsManage,
    CategoriesManage,
    ShelvesManage,
    ReportsView,
    WorklogView,
    SalesUndo,
};

struct FeatureInfo {
    Feature feature;
    const char* key;         // stable storage key (never translated)
    const char* name;        // English display name (translated in the UI)
    int defaultMinRole;      // minimum Domain::User::Role value that gets it by default
};

// The full feature registry, in display order.
const std::vector<FeatureInfo>& registry();

// Look up a feature by enum or by storage key (nullptr when unknown).
const FeatureInfo* info(Feature feature);
const FeatureInfo* infoForKey(const std::string& key);

// Subject kinds used by grants and template assignments.
inline constexpr const char* SubjectUser = "user";
inline constexpr const char* SubjectRole = "role";

// A subject id for a role is its number as text ("1"/"2"/"3"), which
// keeps the stored data independent of the UI language.
std::string roleId(Domain::User::Role role);
Domain::User::Role roleFromId(const std::string& id,
                              Domain::User::Role fallback = Domain::User::Role::UserRole);

// Canonical English role name, e.g. "Admin".
std::string roleKey(Domain::User::Role role);

// Where an effective permission comes from (surfaced in the UI).
enum class Source { Default, RoleTemplate, UserTemplate, RoleGrant, UserGrant };

struct Decision {
    bool allowed = false;
    Source source = Source::Default;
};

// The built-in default for a role, ignoring templates and grants.
bool defaultForRole(Feature feature, Domain::User::Role role);

// Effective decisions for every registered feature (single DB pass).
std::map<std::string, Decision> resolveAll(DataAccess::IDataAccess& db,
                                           const Domain::User& user);

// Effective decision / boolean shortcut for one feature.
Decision decide(DataAccess::IDataAccess& db, const Domain::User& user, Feature feature);
bool can(DataAccess::IDataAccess& db, const Domain::User& user, Feature feature);

} // namespace Permissions

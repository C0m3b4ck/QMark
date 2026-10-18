#include "permissions.h"

#include <algorithm>
#include <map>
#include <set>

namespace Permissions {

const std::vector<FeatureInfo>& registry()
{
    static const std::vector<FeatureInfo> reg = {
        { Feature::Resupply,         "resupply",          "Restock / resupply items",      static_cast<int>(Domain::User::Role::Admin) },
        { Feature::ItemsManage,      "items.manage",      "Add, edit and remove items",    static_cast<int>(Domain::User::Role::Admin) },
        { Feature::CategoriesManage, "categories.manage", "Manage categories",             static_cast<int>(Domain::User::Role::Admin) },
        { Feature::ShelvesManage,    "shelves.manage",    "Manage shelves",                static_cast<int>(Domain::User::Role::Admin) },
        { Feature::ReportsView,      "reports.view",      "View sales history & reports", static_cast<int>(Domain::User::Role::Admin) },
        { Feature::WorklogView,      "worklog.view",      "View the worklog",              static_cast<int>(Domain::User::Role::Admin) },
        { Feature::SalesUndo,        "sales.undo",        "Undo sales",                    static_cast<int>(Domain::User::Role::UserRole) },
    };
    return reg;
}

const FeatureInfo* info(Feature feature)
{
    for (const auto& fi : registry()) {
        if (fi.feature == feature) return &fi;
    }
    return nullptr;
}

const FeatureInfo* infoForKey(const std::string& key)
{
    for (const auto& fi : registry()) {
        if (key == fi.key) return &fi;
    }
    return nullptr;
}

std::string roleId(Domain::User::Role role)
{
    return std::to_string(static_cast<int>(role));
}

Domain::User::Role roleFromId(const std::string& id, Domain::User::Role fallback)
{
    try {
        int v = std::stoi(id);
        switch (v) {
            case 1: return Domain::User::Role::UserRole;
            case 2: return Domain::User::Role::Admin;
            case 3: return Domain::User::Role::SuperAdmin;
            default: break;
        }
    }
    catch (...) {
        // fall through to the fallback
    }
    return fallback;
}

std::string roleKey(Domain::User::Role role)
{
    Domain::User u;
    u.role = role;
    return u.roleName();
}

bool defaultForRole(Feature feature, Domain::User::Role role)
{
    const FeatureInfo* fi = info(feature);
    if (!fi) return false;
    return static_cast<int>(role) >= fi->defaultMinRole;
}

std::map<std::string, Decision> resolveAll(DataAccess::IDataAccess& db,
                                           const Domain::User& user)
{
    std::map<std::string, Decision> result;
    for (const auto& fi : registry()) {
        Decision d;
        d.allowed = defaultForRole(fi.feature, user.role);
        d.source = Source::Default;
        result[fi.key] = d;
    }

    // Load the small permission tables once.
    const auto templates = db.getAllPermissionTemplates();
    const auto assignments = db.getAllTemplateAssignments();
    const auto grants = db.getAllFeatureGrants();

    std::map<std::string, std::set<std::string>> tplFeatures;
    for (const auto& t : templates) {
        tplFeatures[t.id] = std::set<std::string>(t.features.begin(), t.features.end());
    }

    const std::string rid = roleId(user.role);
    std::string roleTpl;
    std::string userTpl;
    for (const auto& a : assignments) {
        if (a.subjectType == SubjectRole && a.subjectId == rid) roleTpl = a.templateId;
        else if (a.subjectType == SubjectUser && a.subjectId == user.username) userTpl = a.templateId;
    }

    auto addTemplate = [&](const std::string& tplId, Source source) {
        if (tplId.empty()) return;
        auto it = tplFeatures.find(tplId);
        if (it == tplFeatures.end()) return;
        for (const auto& key : it->second) {
            auto rit = result.find(key);
            if (rit != result.end()) {
                rit->second.allowed = true;
                rit->second.source = source;
            }
        }
    };

    addTemplate(roleTpl, Source::RoleTemplate);
    addTemplate(userTpl, Source::UserTemplate);

    for (const auto& g : grants) {
        auto rit = result.find(g.feature);
        if (rit == result.end()) continue;
        if (g.subjectType == SubjectRole && g.subjectId == rid) {
            rit->second.allowed = g.enabled;
            rit->second.source = Source::RoleGrant;
        } else if (g.subjectType == SubjectUser && g.subjectId == user.username) {
            rit->second.allowed = g.enabled;
            rit->second.source = Source::UserGrant;
        }
    }

    return result;
}

Decision decide(DataAccess::IDataAccess& db, const Domain::User& user, Feature feature)
{
    const FeatureInfo* fi = info(feature);
    if (!fi) return Decision{};
    auto all = resolveAll(db, user);
    auto it = all.find(fi->key);
    if (it == all.end()) return Decision{};
    return it->second;
}

bool can(DataAccess::IDataAccess& db, const Domain::User& user, Feature feature)
{
    return decide(db, user, feature).allowed;
}

} // namespace Permissions

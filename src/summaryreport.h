#pragma once

// ─────────────────────────────────────────────────────────────────────
// summaryreport.h — daily/monthly shop summary generation for the
// scheduled e-mail. Reuses the GUI-independent Stats engine; produces
// plain text and a lightweight HTML version.
// ─────────────────────────────────────────────────────────────────────

#include <QString>
#include "dataaccess.h"

namespace Summary {

// "Daily" = today's local-day activity; "monthly" = the current calendar
// month. Both include revenue/tx/items totals, today's/month's top seller
// and top item, and a low-stock alert list.
QString buildText(bool monthly, DataAccess::IDataAccess& db);
QString buildHtml(bool monthly, DataAccess::IDataAccess& db);

} // namespace Summary
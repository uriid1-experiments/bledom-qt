#pragma once

#include <QString>
#include <QStringList>

// Runtime language switching. Translations are embedded as Qt resources
// under ":/i18n" (see qt_add_translations in CMakeLists.txt).
namespace Language
{
// Language codes offered in the UI, in display order.
QStringList available();

// Human-readable name for a code (in that language itself).
QString displayName(const QString &code);

// Persisted choice ("" = follow system locale).
QString saved();

// Installs translators for `code` (app + Qt's own) and persists the choice.
// Triggers QEvent::LanguageChange on all widgets.
void apply(const QString &code);
}

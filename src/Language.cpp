#include "Language.h"

#include <QCoreApplication>
#include <QLibraryInfo>
#include <QLocale>
#include <QSettings>
#include <QTranslator>

namespace {
const char *kSettingsKey = "language";
QTranslator *g_app = nullptr;
QTranslator *g_qt = nullptr;

// Returns every source string unchanged. Installed for English so that
// translators added by the platform theme (e.g. KDE loads qtbase_<locale>
// automatically) do not leak the system language into the English UI:
// the most recently installed translator wins as soon as it returns
// a non-null string.
class IdentityTranslator : public QTranslator
{
public:
    using QTranslator::QTranslator;
    bool isEmpty() const override { return false; }
    QString translate(const char *, const char *sourceText, const char *, int) const override
    {
        return QString::fromUtf8(sourceText);
    }
};
}

namespace Language
{

QStringList available()
{
    return {QStringLiteral("en"), QStringLiteral("ru")};
}

QString displayName(const QString &code)
{
    if (code == QLatin1String("ru"))
        return QStringLiteral("Русский");
    return QStringLiteral("English");
}

QString saved()
{
    QSettings st;
    QString code = st.value(QLatin1String(kSettingsKey)).toString();
    if (code.isEmpty()) {
        // First run: pick from the system locale, fall back to English.
        const QString sys = QLocale::system().name().left(2);
        code = available().contains(sys) ? sys : QStringLiteral("en");
    }
    return code;
}

void apply(const QString &code)
{
    QCoreApplication *app = QCoreApplication::instance();

    if (g_app) {
        app->removeTranslator(g_app);
        delete g_app;
        g_app = nullptr;
    }
    if (g_qt) {
        app->removeTranslator(g_qt);
        delete g_qt;
        g_qt = nullptr;
    }

    if (code == QLatin1String("en")) {
        // English is the source language: no app translator needed.
        g_qt = new IdentityTranslator(app);
        app->installTranslator(g_qt);
    } else {
        g_app = new QTranslator(app);
        if (g_app->load(QStringLiteral("bledom-qt_") + code, QStringLiteral(":/i18n")))
            app->installTranslator(g_app);
        else {
            delete g_app;
            g_app = nullptr;
        }

        // Qt's own strings (color dialog buttons etc.).
        g_qt = new QTranslator(app);
        if (g_qt->load(QStringLiteral("qtbase_") + code,
                       QLibraryInfo::path(QLibraryInfo::TranslationsPath)))
            app->installTranslator(g_qt);
        else {
            delete g_qt;
            g_qt = nullptr;
        }
    }

    QSettings st;
    st.setValue(QLatin1String(kSettingsKey), code);
}

} // namespace Language

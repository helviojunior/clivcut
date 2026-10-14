// SPDX-License-Identifier: GPL-3.0-or-later
#include "Language.h"

#include <QCoreApplication>
#include <QLibraryInfo>
#include <QLocale>
#include <QSettings>
#include <QTranslator>
#include <QVariantMap>

namespace clv::app {

namespace {

struct Translators {
    QTranslator app;
    QTranslator qt;
    QString active = QStringLiteral("en");
};

Translators &translators()
{
    // Nunca destruído: threads de trabalho ainda podem traduzir mensagens durante a saída.
    static Translators *t = new Translators;
    return *t;
}

// Inglês é a língua do código: não precisa de arquivo (só plurais).
const char *const kLanguages[][2] = {
    {"en", "English"},
    {"pt_BR", "Português (Brasil)"},
    {"es", "Español"},
};

} // namespace

QString savedLanguage()
{
    return QSettings().value(QStringLiteral("ui/language")).toString();
}

void saveLanguage(const QString &code)
{
    QSettings().setValue(QStringLiteral("ui/language"), code);
}

void installLanguage(const QString &code)
{
    Translators &t = translators();
    QCoreApplication::removeTranslator(&t.app);
    QCoreApplication::removeTranslator(&t.qt);

    const QLocale locale = code.isEmpty() ? QLocale::system() : QLocale(code);
    // Procura clivcut_pt_BR, depois clivcut_pt, para cada idioma preferido do sistema.
    // Sem tradução para o idioma pedido: inglês, cujo arquivo só traz os plurais.
    if (t.app.load(locale, QStringLiteral("clivcut"), QStringLiteral("_"), QStringLiteral(":/i18n"))
        || t.app.load(QStringLiteral(":/i18n/clivcut_en.qm"))) {
        QCoreApplication::installTranslator(&t.app);
        t.active = t.app.language().isEmpty() ? QStringLiteral("en") : t.app.language();
    } else {
        t.active = QStringLiteral("en");
    }
    // Textos do próprio Qt (diálogos de arquivo, menus de contexto), quando instalados.
    if (t.active != QLatin1String("en")
        && t.qt.load(locale, QStringLiteral("qtbase"), QStringLiteral("_"),
                     QLibraryInfo::path(QLibraryInfo::TranslationsPath)))
        QCoreApplication::installTranslator(&t.qt);
    QLocale::setDefault(t.active == QLatin1String("en") ? QLocale(QLocale::English) : QLocale(t.active));
}

QString activeLanguage()
{
    return translators().active;
}

QVariantList availableLanguages()
{
    QVariantList out;
    for (const auto &l : kLanguages)
        out << QVariantMap{{"code", QString::fromLatin1(l[0])}, {"name", QString::fromUtf8(l[1])}};
    return out;
}

} // namespace clv::app

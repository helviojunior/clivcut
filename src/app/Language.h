// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once

#include <QString>
#include <QVariantList>

/**
 * Idioma da interface. Os textos do código estão em inglês; as traduções
 * (i18n/clivcut_*.ts) são compiladas em :/i18n. Código vazio = idioma do sistema.
 */
namespace clv::app {

/// Idioma salvo nas preferências ("" = seguir o sistema).
QString savedLanguage();
void saveLanguage(const QString &code);

/// Instala os tradutores do app e do Qt para `code` (ou o do sistema).
void installLanguage(const QString &code);

/// Idioma efetivamente em uso ("en", "pt_BR", "es"...).
QString activeLanguage();

/// [{code, name}] com o nome de cada idioma na própria língua.
QVariantList availableLanguages();

} // namespace clv::app

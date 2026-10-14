// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once

#include "Model.h"

#include <QByteArray>
#include <QString>
#include <QStringList>

/**
 * Importação (somente leitura) de drafts JSON (draft_content.json) para
 * interoperabilidade.
 *
 * Suporta drafts em texto puro (draft_content.json / draft_info.json).
 * Drafts criptografados não são suportados: são detectados e reportados.
 */
namespace clv {

struct DraftImportResult {
    bool ok = false;
    QString error;
    QStringList warnings;
    Project project;
};

/** `path` pode ser a pasta do draft ou o próprio arquivo JSON. */
DraftImportResult importDraft(const QString &path);

/** Importa a partir do conteúdo JSON; `draftDir` resolve caminhos relativos/placeholder. */
DraftImportResult importDraftJson(const QByteArray &json, const QString &draftDir);

} // namespace clv

// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once

#include <QString>
#include <QUrl>

#include <vector>

namespace clv::engine {

/** Modelos de IA locais baixados sob demanda (transcrição, recorte, voz). */
struct AiModel {
    QString id;
    QString label;
    qint64 sizeBytes;
    QString description;
    QString kind;     ///< "whisper", "matting", "voice"
    QString fileName; ///< nome do arquivo na pasta de modelos
    QUrl url;
    QString license;
};

const std::vector<AiModel> &aiModels();
const AiModel *findAiModel(const QString &id);
QString aiModelPath(const QString &id);
bool aiModelInstalled(const QString &id);

/// Modelo de recorte de pessoas (RobustVideoMatting, GPL-3.0).
inline const QString kMattingModel = QStringLiteral("rvm");

} // namespace clv::engine

// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once

#include <QString>
#include <QUrl>

#include <vector>

namespace clv::engine {

/** Modelos do whisper.cpp (formato ggml) publicados pelo projeto no Hugging Face. */
struct WhisperModel {
    QString id;          ///< "tiny", "base", ...
    QString label;       ///< nome para a interface
    qint64 sizeBytes;    ///< tamanho aproximado do download
    QString description;
};

const std::vector<WhisperModel> &whisperModels();

/** Pasta onde os modelos ficam (~/.local/share/clivcut/models no Linux). */
QString whisperModelsDir();
QString whisperModelPath(const QString &id);
QUrl whisperModelUrl(const QString &id);
bool whisperModelInstalled(const QString &id);

/** Aceita um id do catálogo ou um caminho para um arquivo .bin. */
QString resolveWhisperModel(const QString &idOrPath);

} // namespace clv::engine

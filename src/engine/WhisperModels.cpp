// SPDX-License-Identifier: GPL-3.0-or-later
#include "WhisperModels.h"

#include <QDir>
#include <QFileInfo>
#include <QStandardPaths>

namespace clv::engine {

const std::vector<WhisperModel> &whisperModels()
{
    static const std::vector<WhisperModel> models{
        {QStringLiteral("tiny"), QStringLiteral("Tiny"), 78'000'000, QStringLiteral("Muito rápido, precisão básica")},
        {QStringLiteral("base"), QStringLiteral("Base"), 148'000'000, QStringLiteral("Rápido, bom para fala clara")},
        {QStringLiteral("small"), QStringLiteral("Small"), 488'000'000, QStringLiteral("Equilibrado (recomendado)")},
        {QStringLiteral("large-v3-turbo"), QStringLiteral("Large v3 Turbo"), 1'620'000'000,
         QStringLiteral("Máxima precisão, mais lento")},
    };
    return models;
}

QString whisperModelsDir()
{
    const QString override = qEnvironmentVariable("CLIVCUT_MODELS_DIR");
    if (!override.isEmpty())
        return override;
    QString base = QStandardPaths::writableLocation(QStandardPaths::GenericDataLocation);
    return QDir(base).filePath(QStringLiteral("clivcut/models"));
}

QString whisperModelPath(const QString &id)
{
    return QDir(whisperModelsDir()).filePath(QStringLiteral("ggml-%1.bin").arg(id));
}

QUrl whisperModelUrl(const QString &id)
{
    return QUrl(QStringLiteral("https://huggingface.co/ggerganov/whisper.cpp/resolve/main/ggml-%1.bin").arg(id));
}

bool whisperModelInstalled(const QString &id)
{
    const QFileInfo fi(whisperModelPath(id));
    return fi.exists() && fi.size() > 1'000'000;
}

QString resolveWhisperModel(const QString &idOrPath)
{
    if (idOrPath.contains(QLatin1Char('/')) || idOrPath.endsWith(QLatin1String(".bin")))
        return idOrPath;
    return whisperModelPath(idOrPath);
}

} // namespace clv::engine

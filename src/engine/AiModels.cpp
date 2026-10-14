// SPDX-License-Identifier: GPL-3.0-or-later
#include "AiModels.h"

#include "WhisperModels.h"

#include <QDir>
#include <QFileInfo>

namespace clv::engine {

const std::vector<AiModel> &aiModels()
{
    static const std::vector<AiModel> models = [] {
        std::vector<AiModel> list;
        for (const auto &w : whisperModels())
            list.push_back({w.id, w.label, w.sizeBytes, w.description, QStringLiteral("whisper"),
                            QStringLiteral("ggml-%1.bin").arg(w.id), whisperModelUrl(w.id), QStringLiteral("MIT")});
        list.push_back({kMattingModel, QStringLiteral("Recorte de pessoas (RVM)"), 14'975'696,
                        QStringLiteral("Remove o fundo de vídeos com pessoas, estável entre quadros"),
                        QStringLiteral("matting"), QStringLiteral("rvm_mobilenetv3_fp32.onnx"),
                        QUrl(QStringLiteral("https://github.com/PeterL1n/RobustVideoMatting/releases/download/v1.0.0/"
                                            "rvm_mobilenetv3_fp32.onnx")),
                        QStringLiteral("GPL-3.0")});
        return list;
    }();
    return models;
}

const AiModel *findAiModel(const QString &id)
{
    for (const auto &m : aiModels())
        if (m.id == id)
            return &m;
    return nullptr;
}

QString aiModelPath(const QString &id)
{
    const AiModel *m = findAiModel(id);
    return m ? QDir(whisperModelsDir()).filePath(m->fileName) : QString();
}

bool aiModelInstalled(const QString &id)
{
    const QFileInfo fi(aiModelPath(id));
    return fi.exists() && fi.size() > 1'000'000;
}

} // namespace clv::engine

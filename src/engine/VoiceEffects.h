// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once

#include <QString>

#include <functional>
#include <vector>

/**
 * Efeitos de voz (eco, reverberação, robô, tom agudo/grave, telefone, rádio):
 * cadeias de filtros do FFmpeg aplicadas ao áudio do clipe fora da reprodução,
 * com o resultado em cache e alinhado à mídia, como a redução de ruído.
 */
namespace clv::engine {

struct VoiceEffect {
    QString id;
    QString name; ///< em inglês; traduzir com QCoreApplication::translate("VoiceEffects", ...)
    QByteArray filters;
};

const std::vector<VoiceEffect> &voiceEffects();
const VoiceEffect *findVoiceEffect(const QString &id);

/** Arquivo em cache do efeito aplicado a `sourcePath` (a mídia ou sua versão limpa). */
QString effectPathFor(const QString &sourcePath, const QString &effectId);

bool generateEffect(const QString &sourcePath, const QString &effectId, const QString &outPath,
                    const std::function<bool(double)> &progress, QString *error);

void registerEffect(const QString &sourcePath, const QString &effectId, const QString &outPath);
QString registeredEffect(const QString &sourcePath, const QString &effectId);
bool discoverEffect(const QString &sourcePath, const QString &effectId);

} // namespace clv::engine

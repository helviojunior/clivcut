// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once

#include <QString>

#include <functional>

/**
 * Isolamento de voz por IA local (DeepFilterNet3 em streaming via ONNX
 * Runtime): separa a fala de ruído, música de fundo e ambiente. Como a
 * redução de ruído, o arquivo inteiro é processado em segundo plano e o
 * resultado (WAV) fica em cache, alinhado à mídia original.
 */
namespace clv::engine {

/** Disponível neste build (compilado com ONNX Runtime). */
bool voiceIsolationAvailable();

/** Intensidade quantizada (passos de 0,1). */
double quantizedVoice(double strength);

QString voicePathFor(const QString &mediaPath, double strength);

/** `progress(fração)` retornando false cancela. */
bool generateVoiceIsolated(const QString &mediaPath, const QString &modelPath, const QString &outPath, double strength,
                           const std::function<bool(double)> &progress, QString *error);

/** Registro (thread-safe) dos áudios prontos: (mídia, intensidade) → arquivo. */
void registerVoice(const QString &mediaPath, double strength, const QString &outPath);
QString registeredVoice(const QString &mediaPath, double strength);
bool discoverVoice(const QString &mediaPath, double strength);

} // namespace clv::engine

// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once

#include <QString>

#include <functional>

/**
 * Redução de ruído de áudio feita fora da reprodução: o arquivo inteiro é
 * processado (filtro FFT afftdn + passa-altas) e guardado em cache como WAV.
 * Processar offline evita a latência do filtro, que desalinharia áudio e vídeo.
 */
namespace clv::engine {

/** Intensidade quantizada (passos de 0,1) para reaproveitar o cache. */
double quantizedDenoise(double strength);

/** Caminho do áudio limpo em cache para um arquivo e intensidade. */
QString denoisedPathFor(const QString &mediaPath, double strength);

bool generateDenoised(const QString &mediaPath, const QString &outPath, double strength,
                      const std::function<bool(double)> &progress, QString *error);

/**
 * Passa o áudio de `inPath` por uma cadeia de filtros do FFmpeg (sintaxe do -af) e grava um WAV
 * de mesma duração, com a latência da cadeia compensada (efeitos de voz).
 */
bool renderAudioFilter(const QString &inPath, const QString &outPath, const QByteArray &chain,
                       const std::function<bool(double)> &progress, QString *error);

/** Registro (thread-safe) de áudios limpos prontos: (mídia, intensidade) → arquivo. */
void registerDenoised(const QString &mediaPath, double strength, const QString &outPath);
QString registeredDenoised(const QString &mediaPath, double strength);
/** Registra se já existir no cache. */
bool discoverDenoised(const QString &mediaPath, double strength);

} // namespace clv::engine

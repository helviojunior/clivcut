// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once

#include <QString>

#include <functional>

/**
 * Remoção de fundo por IA local (RobustVideoMatting via ONNX Runtime).
 * O vídeo inteiro é processado em segundo plano e o alfa vira um "vídeo de
 * matte" em cache (cinza = opacidade), alinhado quadro a quadro com a mídia.
 * O compositor usa o matte como canal alfa no preview e no export.
 */
namespace clv::engine {

constexpr int kMatteMaxSide = 960;

bool mattingAvailable();

QString mattePathFor(const QString &mediaPath);

bool generateMatte(const QString &mediaPath, const QString &modelPath, const QString &outPath,
                   const std::function<bool(double)> &progress, QString *error);

void registerMatte(const QString &mediaPath, const QString &mattePath);
QString registeredMatte(const QString &mediaPath);
bool discoverMatte(const QString &mediaPath);

} // namespace clv::engine

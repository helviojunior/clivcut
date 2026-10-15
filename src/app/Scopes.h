// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once

#include <QImage>
#include <QSize>

#include <array>

/**
 * Escopos de vídeo calculados sobre um quadro reduzido (RGBA): histograma,
 * forma de onda de luminância e vetorscópio. Cada um vira uma imagem do
 * tamanho pedido, pronta para desenhar.
 */
namespace clv::app {

enum class ScopeMode { Histogram = 0, Waveform = 1, Vectorscope = 2 };

/// Contagens do histograma: R, G, B e luminância (BT.709), 256 faixas cada.
std::array<std::array<int, 256>, 4> histogram(const QImage &frame);

QImage renderScope(const QImage &frame, ScopeMode mode, QSize size);

} // namespace clv::app

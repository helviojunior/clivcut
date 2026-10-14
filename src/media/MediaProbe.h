// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once

#include "core/Model.h"

#include <QImage>
#include <QString>

#include <vector>

namespace clv::media {

/** Inspeciona um arquivo e preenche um MediaAsset (sem id). */
bool probe(const QString &path, MediaAsset &out, QString *error = nullptr);

/** Miniatura de um vídeo/imagem no tempo `t`, com largura máxima `maxWidth`. */
QImage thumbnail(const QString &path, Micros t, int maxWidth = 320);

/**
 * Picos de áudio (0..1) em `buckets` intervalos ao longo de toda a mídia,
 * para desenhar a forma de onda na timeline.
 */
std::vector<float> waveform(const QString &path, int buckets);

} // namespace clv::media

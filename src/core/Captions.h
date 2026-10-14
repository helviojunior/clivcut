// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once

#include "Model.h"

#include <QString>

#include <vector>

/**
 * Legendas: conversão SRT/WebVTT e criação de faixas de legenda (clipes de
 * texto), como nas legendas automáticas.
 */
namespace clv {

struct CaptionSegment {
    Micros start = 0;
    Micros end = 0;
    QString text;
};

/** Lê SRT ou WebVTT. Entradas inválidas são ignoradas. */
std::vector<CaptionSegment> parseSubtitles(const QString &content);

QString toSrt(const std::vector<CaptionSegment> &segments);

/** Segmentos a partir dos clipes de texto de uma faixa. */
std::vector<CaptionSegment> captionsFromTrack(const Track &track);

/** Estilo padrão de legenda (texto branco sobre caixa translúcida, terço inferior). */
TextStyle captionStyle();
Transform captionTransform();

/**
 * Cria uma faixa de legendas com um clipe de texto por segmento. Segmentos
 * sobrepostos são aparados para não colidirem. Retorna o índice da faixa.
 */
int addCaptionTrack(Project &p, const std::vector<CaptionSegment> &segments, const QString &name = {});

/**
 * Quebra segmentos longos em pedaços de até `maxChars` caracteres, dividindo o
 * tempo proporcionalmente ao texto (útil para SRT importado sem quebras).
 */
std::vector<CaptionSegment> reflowCaptions(const std::vector<CaptionSegment> &segments, int maxChars);

} // namespace clv

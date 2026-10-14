// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once

#include "Model.h"

#include <QString>

#include <vector>

/**
 * Curvas de velocidade (rampas). A curva v(u) descreve a velocidade ao longo
 * do clipe; o trecho de origem consumido é fixo, então a duração na timeline
 * é S / média(v) e o tempo de origem é proporcional à integral da curva.
 */
namespace clv {

/** Média de v em [0,1] (integral da curva linear por partes). */
double curveMean(const std::vector<SpeedPoint> &curve);

/** Integral normalizada F(u)/F(1) ∈ [0,1]: fração da origem consumida até u. */
double curveProgress(const std::vector<SpeedPoint> &curve, double u);

/** Velocidade instantânea v(u). */
double curveValue(const std::vector<SpeedPoint> &curve, double u);

/** Velocidade instantânea do clipe num tempo relativo (constante sem curva). */
double speedAt(const Clip &c, Micros rel);

/** Ordena, garante pontos em u=0 e u=1 e limita v a [0,1; 10]. */
std::vector<SpeedPoint> normalizedCurve(std::vector<SpeedPoint> curve);

/** Recorta a curva no intervalo [a,b] ⊂ [0,1], reescalando u para [0,1]. */
std::vector<SpeedPoint> sliceCurve(const std::vector<SpeedPoint> &curve, double a, double b);

struct SpeedPreset {
    QString id;
    QString name;
    std::vector<SpeedPoint> points;
};
const std::vector<SpeedPreset> &speedPresets();

} // namespace clv

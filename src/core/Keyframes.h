// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once

#include "Model.h"

#include <utility>
#include <vector>

namespace clv {

double applyEasing(Easing e, double u);
/** cubic-bezier(x1, y1, x2, y2) avaliada em u ∈ [0,1] (x1, x2 limitados a [0,1]). */
double cubicBezier(double x1, double y1, double x2, double y2, double u);
/** Progresso do trecho a→b (u ∈ [0,1]) segundo a curva do keyframe `a`. */
double segmentEase(const Keyframe &a, double u);

/** Valor interpolado de uma lista ordenada; `base` quando vazia. */
double valueAt(const std::vector<Keyframe> &kfs, Micros t, double base);

/** Valor base (sem keyframes) de uma propriedade. */
double baseValue(const Clip &c, Prop p);
void setBaseValue(Clip &c, Prop p, double v);

/** Valor efetivo de uma propriedade em tempo relativo ao início do clipe. */
double propAt(const Clip &c, Prop p, Micros rel);

/** Insere/substitui o keyframe em `kf.t` (tolerância em µs). */
void upsertKeyframe(std::vector<Keyframe> &kfs, Keyframe kf, Micros tolerance = 1000);
bool removeKeyframeAt(std::vector<Keyframe> &kfs, Micros t, Micros tolerance = 1000);
const Keyframe *keyframeAt(const std::vector<Keyframe> &kfs, Micros t, Micros tolerance = 1000);

/**
 * Divide keyframes em `at` (relativo ao clipe). Cada lado recebe um keyframe
 * no limite com o valor interpolado, preservando a animação visível.
 */
std::pair<std::vector<Keyframe>, std::vector<Keyframe>> splitKeyframes(const std::vector<Keyframe> &kfs, Micros at);

/** Descarta keyframes fora de [0, duration] mantendo o valor nas bordas. */
void clampKeyframes(std::vector<Keyframe> &kfs, Micros duration);

/** Desloca keyframes (ex.: ao aparar o início do clipe). */
void shiftKeyframes(std::vector<Keyframe> &kfs, Micros delta);

const char *propName(Prop p);

} // namespace clv

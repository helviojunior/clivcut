// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once

#include "core/Model.h"

#include <functional>
#include <utility>
#include <vector>

namespace clv::engine {

struct SilenceOptions {
    double thresholdDb = -40;    ///< abaixo disto (RMS em janelas de 20 ms) é silêncio
    Micros minSilence = 500'000; ///< pausas menores são mantidas
    Micros padding = 120'000;    ///< respiro mantido em cada lado da fala
    Micros rangeStart = 0;
    Micros rangeEnd = -1;        ///< -1 = até o fim do projeto
};

/**
 * Trechos de silêncio da mixagem da timeline (o que se ouve), já com o
 * respiro descontado — prontos para edit::removeRanges.
 */
std::vector<std::pair<Micros, Micros>> detectSilences(const Project &p, const SilenceOptions &opts,
                                                      const std::function<bool(double)> &progress = {});

/** Mesma detecção sobre amostras mono 16 kHz que começam em `origin` (testes). */
std::vector<std::pair<Micros, Micros>> detectSilencesPcm16k(const std::vector<float> &pcm, Micros origin,
                                                            const SilenceOptions &opts);

} // namespace clv::engine

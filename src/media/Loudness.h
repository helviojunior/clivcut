// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once

#include "core/Model.h"

#include <QString>

#include <cmath>
#include <functional>
#include <vector>

namespace clv::media {

/**
 * Medidor de loudness integrada (ITU-R BS.1770-4 / EBU R128): filtro K,
 * blocos de 400 ms com 75% de sobreposição, portas absoluta (-70 LUFS) e
 * relativa (-10 LU). Entrada: float estéreo intercalado 48 kHz.
 */
class LoudnessMeter {
public:
    void add(const float *interleaved, int frames);
    /** Loudness integrada em LUFS (-inf se só silêncio). */
    double integrated() const;
    /** Maior pico de amostra (linear). */
    double peak() const { return m_peak; }

private:
    struct Biquad {
        double b0, b1, b2, a1, a2;
        double z1 = 0, z2 = 0;
        double process(double x)
        {
            const double y = b0 * x + z1;
            z1 = b1 * x - a1 * y + z2;
            z2 = b2 * x - a2 * y;
            return y;
        }
    };
    struct Channel {
        Biquad shelf{1.53512485958697, -2.69169618940638, 1.19839281085285, -1.69065929318241, 0.73248077421585};
        Biquad highpass{1.0, -2.0, 1.0, -1.99004745483398, 0.99007225036621};
    };
    Channel m_ch[2];
    std::vector<double> m_hopEnergy; ///< energia (soma dos quadrados) de cada salto de 100 ms
    double m_acc = 0;
    int m_accFrames = 0;
    double m_peak = 0;
};

/**
 * Mede a loudness de um trecho de um arquivo (µs na mídia). `progress`
 * retornando false cancela (resultado NaN).
 */
double measureLoudness(const QString &path, Micros start, Micros duration,
                       const std::function<bool(double)> &progress = {}, double *peak = nullptr);

inline double dbToGain(double db) { return std::pow(10.0, db / 20.0); }

} // namespace clv::media

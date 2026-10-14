// SPDX-License-Identifier: GPL-3.0-or-later
#include "TimeStretch.h"

#include <algorithm>
#include <cmath>
#include <cstring>

namespace clv::engine {

namespace {
constexpr int kCh = 2;
}

TimeStretch::TimeStretch()
{
    m_window.resize(kWindow);
    for (int i = 0; i < kWindow; ++i)
        m_window[i] = 0.5f - 0.5f * std::cos(2.0 * M_PI * i / kWindow);
    reset();
}

void TimeStretch::reset()
{
    m_tail.assign(static_cast<size_t>(kHop) * kCh, 0.f);
    m_ready.clear();
    m_readyPos = 0;
    m_anaPos = 0;
    m_prevStart = -1;
    m_in.clear();
    m_inStart = 0;
}

const float *TimeStretch::input(std::int64_t start, int frames, const Reader &read)
{
    // Índices negativos não existem no fluxo: o chamador nunca pede antes de 0.
    start = std::max<std::int64_t>(start, 0);
    const std::int64_t end = start + frames;
    std::int64_t have = m_inStart + static_cast<std::int64_t>(m_in.size() / kCh);
    if (start < m_inStart) {
        // Não deveria acontecer (leitura sempre para frente); recomeça o cache.
        m_in.clear();
        m_inStart = start;
        have = start;
    }
    // Descarta o que ficou bem para trás (mantém margem para a busca).
    const std::int64_t keepFrom = start - kSearch;
    if (keepFrom > m_inStart) {
        const std::int64_t drop = std::min<std::int64_t>(keepFrom - m_inStart, m_in.size() / kCh);
        m_in.erase(m_in.begin(), m_in.begin() + drop * kCh);
        m_inStart += drop;
    }
    if (end > have) {
        const int need = static_cast<int>(end - have);
        const size_t old = m_in.size();
        m_in.resize(old + static_cast<size_t>(need) * kCh);
        read(have, m_in.data() + old, need);
    }
    return m_in.data() + (start - m_inStart) * kCh;
}

void TimeStretch::synthesizeHop(double speed, const Reader &read)
{
    std::int64_t start;
    if (m_prevStart < 0) {
        // Primeira janela: sem nada para alinhar; a "janela anterior virtual" é o próprio sinal,
        // o que torna o começo idêntico à entrada (sem fade-in).
        start = static_cast<std::int64_t>(std::llround(m_anaPos));
        const float *x = input(start, kWindow, read);
        for (int i = 0; i < kHop; ++i)
            for (int c = 0; c < kCh; ++c)
                m_tail[i * kCh + c] = x[i * kCh + c] * m_window[i + kHop];
    } else {
        // Continuação natural da janela anterior e região de busca em torno da posição nominal.
        const std::int64_t natural = m_prevStart + kHop;
        const std::int64_t nominal = static_cast<std::int64_t>(std::llround(m_anaPos));
        const std::int64_t lo = std::max<std::int64_t>(0, nominal - kSearch);
        const std::int64_t hi = nominal + kSearch;
        const std::int64_t from = std::min(lo, natural);
        const std::int64_t to = std::max(hi, natural) + kWindow;
        const float *base = input(from, static_cast<int>(to - from), read);
        auto mono = [&](std::int64_t idx) {
            const float *p = base + (idx - from) * kCh;
            return p[0] + p[1];
        };
        // Correlação normalizada sobre a meia janela de sobreposição (amostrada a cada 4).
        start = nominal;
        if (natural != nominal) {
            double best = -1e30;
            for (std::int64_t cand = lo; cand <= hi; cand += 2) {
                double num = 0, energy = 1e-9;
                for (int i = 0; i < kHop; i += 4) {
                    const float b = mono(cand + i);
                    num += mono(natural + i) * b;
                    energy += b * b;
                }
                const double score = num / std::sqrt(energy);
                if (score > best) {
                    best = score;
                    start = cand;
                }
            }
        }
    }

    const float *x = input(start, kWindow, read);
    const size_t old = m_ready.size();
    m_ready.resize(old + static_cast<size_t>(kHop) * kCh);
    float *dst = m_ready.data() + old;
    for (int i = 0; i < kHop; ++i)
        for (int c = 0; c < kCh; ++c) {
            dst[i * kCh + c] = m_tail[i * kCh + c] + x[i * kCh + c] * m_window[i];
            m_tail[i * kCh + c] = x[(i + kHop) * kCh + c] * m_window[i + kHop];
        }
    m_prevStart = start;
    m_anaPos += kHop * speed;
}

void TimeStretch::process(float *out, int frames, double speed, const Reader &read)
{
    speed = std::clamp(speed, 0.05, 20.0);
    int written = 0;
    while (written < frames) {
        if (m_readyPos >= m_ready.size()) {
            m_ready.clear();
            m_readyPos = 0;
            synthesizeHop(speed, read);
        }
        const int avail = static_cast<int>((m_ready.size() - m_readyPos) / kCh);
        const int n = std::min(avail, frames - written);
        std::memcpy(out + written * kCh, m_ready.data() + m_readyPos, sizeof(float) * n * kCh);
        m_readyPos += static_cast<size_t>(n) * kCh;
        written += n;
    }
}

} // namespace clv::engine

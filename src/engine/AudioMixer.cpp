// SPDX-License-Identifier: GPL-3.0-or-later
#include "AudioMixer.h"

#include "core/Evaluate.h"
#include "core/SpeedCurve.h"

#include <algorithm>
#include <cmath>
#include <cstring>

namespace clv::engine {

using media::kChannels;
using media::kSampleRate;

media::AudioDecoder *AudioMixer::decoderFor(const Clip &c, const MediaAsset &a)
{
    Entry &e = m_decoders[c.id];
    if (e.path != a.path) {
        e = Entry{};
        e.path = a.path;
    }
    if (e.failed)
        return nullptr;
    if (!e.dec) {
        e.dec = std::make_unique<media::AudioDecoder>();
        if (!e.dec->open(a.path)) {
            e.failed = true;
            e.dec.reset();
            return nullptr;
        }
    }
    return e.dec.get();
}

void AudioMixer::mix(const Project &p, Micros t, float *out, int frames)
{
    std::memset(out, 0, sizeof(float) * frames * kChannels);
    const Micros span = static_cast<Micros>(static_cast<double>(frames) * kSecond / kSampleRate);
    const Micros tEnd = t + span;

    for (const Track &track : p.tracks) {
        if (track.muted || track.kind == TrackKind::Text)
            continue;
        for (const Clip &c : track.clips) {
            if (!c.hasMedia() || c.audioMuted || c.end() <= t || c.start >= tEnd || c.reverse || c.speed <= 0)
                continue;
            const MediaAsset *a = p.asset(c.assetId);
            if (!a || !a->hasAudio)
                continue;
            media::AudioDecoder *dec = decoderFor(c, *a);
            if (!dec)
                continue;

            // Trecho do bloco coberto por este clipe.
            const Micros segStart = std::max(t, c.start);
            const Micros segEnd = std::min(tEnd, c.end());
            const int off = static_cast<int>(std::llround(static_cast<double>(segStart - t) * kSampleRate / kSecond));
            const int n = std::min(frames - off,
                                   static_cast<int>(std::llround(static_cast<double>(segEnd - segStart) * kSampleRate / kSecond)));
            if (n <= 0)
                continue;

            const Micros rel = segStart - c.start;
            const Micros src = sourceTimeAt(p, c, rel);
            const double speed = speedAt(c, rel); // instantânea (curvas mudam ao longo do clipe)
            const int srcFrames = std::max(1, static_cast<int>(std::ceil(n * speed)) + 1);
            m_tmp.resize(static_cast<size_t>(srcFrames) * kChannels);
            dec->read(src, m_tmp.data(), srcFrames);

            const double g0 = audioGainAt(c, rel);
            const double g1 = audioGainAt(c, rel + (segEnd - segStart));
            for (int i = 0; i < n; ++i) {
                const double g = g0 + (g1 - g0) * (static_cast<double>(i) / n);
                float l, r;
                if (speed == 1.0) {
                    l = m_tmp[i * 2];
                    r = m_tmp[i * 2 + 1];
                } else {
                    const double pos = i * speed;
                    const int i0 = std::min(static_cast<int>(pos), srcFrames - 2);
                    const float fr = static_cast<float>(pos - i0);
                    l = m_tmp[i0 * 2] * (1 - fr) + m_tmp[(i0 + 1) * 2] * fr;
                    r = m_tmp[i0 * 2 + 1] * (1 - fr) + m_tmp[(i0 + 1) * 2 + 1] * fr;
                }
                out[(off + i) * 2] += static_cast<float>(l * g);
                out[(off + i) * 2 + 1] += static_cast<float>(r * g);
            }
        }
    }
    // Limitador suave para evitar clipping duro na soma.
    for (int i = 0; i < frames * kChannels; ++i) {
        const float x = out[i];
        if (x > 1.f || x < -1.f)
            out[i] = std::tanh(x);
    }
}

void AudioMixer::prune(const QSet<QString> &liveClipIds)
{
    for (auto it = m_decoders.begin(); it != m_decoders.end();) {
        if (!liveClipIds.contains(it->first))
            it = m_decoders.erase(it);
        else
            ++it;
    }
}

} // namespace clv::engine

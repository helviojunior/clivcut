// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once

#include "core/Model.h"
#include "media/AudioDecoder.h"

#include <unordered_map>
#include <QSet>

#include <memory>
#include <vector>

namespace clv::engine {

/**
 * Mixa todas as fontes de áudio do projeto num intervalo da timeline.
 * Saída: float estéreo intercalado 48 kHz. Aplica volume (com keyframes),
 * fades e velocidade (reamostragem simples, altera o tom; pitch-lock virá
 * com Rubber Band/SoundTouch). Clipes em reverso ficam mudos por enquanto.
 *
 * Não é thread-safe: use uma instância por thread.
 */
class AudioMixer {
public:
    /** Mixa `frames` quadros começando em `t` (µs da timeline) em `out`. */
    void mix(const Project &p, Micros t, float *out, int frames);
    void prune(const QSet<QString> &liveClipIds);
    void clear() { m_decoders.clear(); }

private:
    struct Entry {
        std::unique_ptr<media::AudioDecoder> dec;
        QString path;
        bool failed = false;
    };
    media::AudioDecoder *decoderFor(const Clip &c, const MediaAsset &a);

    std::unordered_map<QString, Entry> m_decoders;
    std::vector<float> m_tmp;
};

} // namespace clv::engine

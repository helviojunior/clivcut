// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once

#include "FFmpegUtil.h"
#include "core/Model.h"

#include <QString>

#include <vector>

namespace clv::media {

constexpr int kSampleRate = 48000;
constexpr int kChannels = 2;

/**
 * Decodificador de áudio com saída fixa: 48 kHz, estéreo, float intercalado.
 * Leituras contíguas são sequenciais; descontinuidades fazem seek.
 * Não é thread-safe: use uma instância por thread.
 */
class AudioDecoder {
public:
    bool open(const QString &path, QString *error = nullptr);
    bool isOpen() const { return bool(m_fmt); }
    Micros duration() const { return m_duration; }

    /**
     * Lê `frames` amostras estéreo a partir de `t` (µs na mídia) para `out`
     * (2*frames floats). Silêncio após o fim. Retorna quantos quadros vieram da mídia.
     */
    int read(Micros t, float *out, int frames);

private:
    void seekTo(Micros t);
    bool decodeMore();

    FormatCtxPtr m_fmt;
    CodecCtxPtr m_dec;
    SwrPtr m_swr;
    PacketPtr m_pkt;
    FramePtr m_frame;
    int m_stream = -1;
    AVRational m_tb{1, 1};
    Micros m_duration = 0;
    Micros m_startTime = 0;
    bool m_eof = false;

    std::vector<float> m_fifo; ///< amostras intercaladas já convertidas
    std::int64_t m_fifoStart = 0; ///< índice (em quadros de 48 kHz) da 1ª amostra do fifo
    std::int64_t m_expected = -1; ///< próximo quadro esperado numa leitura sequencial
};

} // namespace clv::media

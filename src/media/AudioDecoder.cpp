// SPDX-License-Identifier: GPL-3.0-or-later
#include "AudioDecoder.h"

extern "C" {
#include <libavutil/channel_layout.h>
#include <libavutil/opt.h>
}

#include <algorithm>
#include <cstring>

namespace clv::media {

namespace {

std::int64_t microsToFrames(Micros t)
{
    return av_rescale(t, kSampleRate, kSecond);
}

} // namespace

bool AudioDecoder::open(const QString &path, QString *error)
{
    m_fmt = openInput(path, error);
    if (!m_fmt)
        return false;
    m_stream = av_find_best_stream(m_fmt.get(), AVMEDIA_TYPE_AUDIO, -1, -1, nullptr, 0);
    if (m_stream < 0) {
        if (error)
            *error = QStringLiteral("Sem stream de áudio");
        m_fmt.reset();
        return false;
    }
    AVStream *st = m_fmt->streams[m_stream];
    m_dec = openDecoder(st, error);
    if (!m_dec) {
        m_fmt.reset();
        return false;
    }
    m_tb = st->time_base;
    m_startTime = st->start_time != AV_NOPTS_VALUE ? streamToMicros(st->start_time, m_tb) : 0;
    m_duration = st->duration != AV_NOPTS_VALUE ? streamToMicros(st->duration, m_tb) : std::max<int64_t>(0, m_fmt->duration);

    AVChannelLayout outLayout = AV_CHANNEL_LAYOUT_STEREO;
    SwrContext *swr = nullptr;
    if (m_dec->ch_layout.nb_channels == 0)
        av_channel_layout_default(&m_dec->ch_layout, 2);
    int err = swr_alloc_set_opts2(&swr, &outLayout, AV_SAMPLE_FMT_FLT, kSampleRate, &m_dec->ch_layout,
                                  m_dec->sample_fmt, m_dec->sample_rate, 0, nullptr);
    if (err < 0 || swr_init(swr) < 0) {
        swr_free(&swr);
        if (error)
            *error = QStringLiteral("Falha ao configurar reamostragem de áudio");
        m_fmt.reset();
        return false;
    }
    m_swr.reset(swr);
    m_pkt.reset(av_packet_alloc());
    m_frame.reset(av_frame_alloc());
    return true;
}

void AudioDecoder::seekTo(Micros t)
{
    const Micros target = std::max<Micros>(0, t) + m_startTime;
    av_seek_frame(m_fmt.get(), m_stream, microsToStream(target, m_tb), AVSEEK_FLAG_BACKWARD);
    avcodec_flush_buffers(m_dec.get());
    swr_init(m_swr.get());
    m_eof = false;
    m_fifo.clear();
    m_fifoStart = -1; // definido pelo pts do primeiro quadro decodificado
}

bool AudioDecoder::decodeMore()
{
    for (;;) {
        int err = avcodec_receive_frame(m_dec.get(), m_frame.get());
        if (err == 0) {
            if (m_fifoStart < 0) {
                const std::int64_t ts = m_frame->best_effort_timestamp != AV_NOPTS_VALUE ? m_frame->best_effort_timestamp
                                                                                       : m_frame->pts;
                const Micros pts = ts == AV_NOPTS_VALUE ? 0 : streamToMicros(ts, m_tb) - m_startTime;
                m_fifoStart = microsToFrames(pts);
            }
            const int maxOut = swr_get_out_samples(m_swr.get(), m_frame->nb_samples);
            const std::size_t old = m_fifo.size();
            m_fifo.resize(old + static_cast<std::size_t>(maxOut) * kChannels);
            uint8_t *outPtr = reinterpret_cast<uint8_t *>(m_fifo.data() + old);
            const int got = swr_convert(m_swr.get(), &outPtr, maxOut, const_cast<const uint8_t **>(m_frame->extended_data),
                                        m_frame->nb_samples);
            m_fifo.resize(old + static_cast<std::size_t>(std::max(0, got)) * kChannels);
            av_frame_unref(m_frame.get());
            return true;
        }
        if (err == AVERROR_EOF || (err != AVERROR(EAGAIN)))
            return false;
        if (m_eof)
            return false;
        err = av_read_frame(m_fmt.get(), m_pkt.get());
        if (err < 0) {
            m_eof = true;
            avcodec_send_packet(m_dec.get(), nullptr);
            continue;
        }
        if (m_pkt->stream_index == m_stream)
            avcodec_send_packet(m_dec.get(), m_pkt.get());
        av_packet_unref(m_pkt.get());
    }
}

int AudioDecoder::read(Micros t, float *out, int frames)
{
    std::memset(out, 0, sizeof(float) * frames * kChannels);
    if (!m_fmt || frames <= 0)
        return 0;
    const std::int64_t want = microsToFrames(t);
    // Tolerância de alguns ms para leitura contínua (arredondamentos do relógio).
    const bool sequential = m_expected >= 0 && std::llabs(want - m_expected) < kSampleRate / 50;
    const std::int64_t start = sequential ? m_expected : want;
    if (!sequential) {
        const bool inFifo = m_fifoStart >= 0 && want >= m_fifoStart &&
                            want < m_fifoStart + static_cast<std::int64_t>(m_fifo.size() / kChannels) + kSampleRate / 4;
        if (!inFifo)
            seekTo(t);
    }

    // Garante amostras até start + frames.
    while (m_fifoStart < 0 || m_fifoStart + static_cast<std::int64_t>(m_fifo.size() / kChannels) < start + frames) {
        if (!decodeMore())
            break;
    }
    if (m_fifoStart < 0)
        return 0;

    // Descarta o que ficou para trás.
    if (start > m_fifoStart) {
        const std::int64_t drop = std::min<std::int64_t>(start - m_fifoStart, m_fifo.size() / kChannels);
        m_fifo.erase(m_fifo.begin(), m_fifo.begin() + drop * kChannels);
        m_fifoStart += drop;
    }
    const std::int64_t offset = std::max<std::int64_t>(0, m_fifoStart - start); // silêncio inicial
    const std::int64_t avail = static_cast<std::int64_t>(m_fifo.size() / kChannels);
    const std::int64_t n = std::clamp<std::int64_t>(frames - offset, 0, avail);
    std::memcpy(out + offset * kChannels, m_fifo.data(), sizeof(float) * n * kChannels);
    m_expected = start + frames;
    return static_cast<int>(n);
}

} // namespace clv::media

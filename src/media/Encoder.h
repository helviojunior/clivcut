// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once

#include "FFmpegUtil.h"

#include <QImage>
#include <QString>
#include <QStringList>

#include <vector>

namespace clv::media {

enum class VideoCodec { H264, HEVC, AV1, ProRes };

struct EncodeSettings {
    int width = 1920;
    int height = 1080;
    double fps = 30;
    VideoCodec codec = VideoCodec::H264;
    /// Qualidade constante (CRF/QP). Ignorado se bitrateKbps > 0.
    int crf = 20;
    int bitrateKbps = 0;
    bool audio = true;
    int audioBitrateKbps = 192;
    QString preset = QStringLiteral("medium");
    int gopFrames = 0; ///< intervalo entre keyframes (0 = 2 s)
};

/** Codecs de vídeo que este FFmpeg consegue codificar. */
QStringList availableVideoCodecs();

/**
 * Encoder de arquivo (MP4/MOV/MKV conforme a extensão) com libavcodec.
 * Vídeo entra como RGBA (alfa pré-multiplicado ignorado) e áudio como
 * float estéreo intercalado 48 kHz.
 */
class Encoder {
public:
    Encoder() = default;
    ~Encoder();
    Encoder(const Encoder &) = delete;
    Encoder &operator=(const Encoder &) = delete;

    bool open(const QString &path, const EncodeSettings &s, QString *error = nullptr);
    bool writeVideo(const QImage &rgba);
    bool writeAudio(const float *interleaved, int frames);
    bool finish(QString *error = nullptr);
    QString codecName() const { return m_codecName; }

private:
    bool encode(AVCodecContext *ctx, AVStream *st, AVFrame *f);
    bool flushAudioFifo(bool final);

    AVFormatContext *m_out = nullptr; // contexto de saída
    bool m_headerWritten = false;
    CodecCtxPtr m_venc;
    CodecCtxPtr m_aenc;
    AVStream *m_vst = nullptr;
    AVStream *m_ast = nullptr;
    SwsPtr m_sws;
    FramePtr m_vframe;
    FramePtr m_aframe;
    PacketPtr m_pkt;
    std::int64_t m_videoPts = 0;
    std::int64_t m_audioPts = 0;
    std::vector<float> m_audioFifo;
    QString m_codecName;
    QString m_lastError;
    bool m_finished = false;
};

} // namespace clv::media

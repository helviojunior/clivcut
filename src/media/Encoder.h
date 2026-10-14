// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once

#include "FFmpegUtil.h"

#include <QImage>
#include <QString>
#include <QStringList>

#include <vector>

struct AVBufferRef;

namespace clv::media {

/// ProRes4444 e VP9Alpha preservam a transparência (canal alfa).
enum class VideoCodec { H264, HEVC, AV1, ProRes, ProRes4444, VP9Alpha };

struct EncodeSettings {
    int width = 1920;
    int height = 1080;
    double fps = 30;
    bool video = true; ///< false = só áudio (formato pela extensão: wav, flac, m4a, opus, mp3)
    VideoCodec codec = VideoCodec::H264;
    /// Tenta primeiro um encoder de hardware (VA-API, NVENC, QSV, VideoToolbox, AMF); cai para software.
    bool hardware = false;
    /// Qualidade constante (CRF/QP). Ignorado se bitrateKbps > 0.
    int crf = 20;
    int bitrateKbps = 0;
    bool audio = true;
    int audioBitrateKbps = 192;
    QString preset = QStringLiteral("medium");
    int gopFrames = 0; ///< intervalo entre keyframes (0 = 2 s)
};

/** Nome curto do codec ("h264", "prores4444"...) e o inverso. */
QString videoCodecName(VideoCodec c);
VideoCodec videoCodecFromName(const QString &name);
bool codecHasAlpha(VideoCodec c);
/** Extensão de arquivo adequada ao codec ("mp4", "mov", "webm"). */
QString containerFor(VideoCodec c);

/** Codecs de vídeo que este FFmpeg consegue codificar. */
QStringList availableVideoCodecs();
/** Codecs com um encoder de hardware que de fato abre nesta máquina (sondado uma vez). */
QStringList availableHardwareCodecs();
/** Formatos de exportação só de áudio disponíveis ("wav", "flac", "m4a", "opus", "mp3"). */
QStringList availableAudioFormats();

/**
 * Encoder de arquivo (contêiner pela extensão) com libavcodec.
 * Vídeo entra como RGBA pré-multiplicado (o alfa só é usado pelos codecs com
 * transparência) e áudio como float estéreo intercalado 48 kHz.
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
    /** Encoder de vídeo em uso ("h264_vaapi", "libx264"...) ou de áudio, se só áudio. */
    QString codecName() const { return m_codecName; }
    bool usingHardware() const { return m_hwFrames != nullptr || m_hwDirect; }
    /** Só abre e fecha o encoder de vídeo (sondagem de hardware). */
    bool tryVideoProbe(const AVCodec *codec, const char *name, AVPixelFormat pixfmt, const EncodeSettings &s);

private:
    struct Choice {
        const char *encoder;
        AVPixelFormat pixfmt;
    };
    bool openVideo(const EncodeSettings &s, QString *error);
    bool tryVideo(const AVCodec *codec, const Choice &choice, const EncodeSettings &s);
    bool openAudio(const QString &path, const EncodeSettings &s, QString *error);
    bool encode(AVCodecContext *ctx, AVStream *st, AVFrame *f);
    bool flushAudioFifo(bool final);

    AVFormatContext *m_out = nullptr; // contexto de saída
    bool m_headerWritten = false;
    CodecCtxPtr m_venc;
    CodecCtxPtr m_aenc;
    AVStream *m_vst = nullptr;
    AVStream *m_ast = nullptr;
    SwsPtr m_sws;
    FramePtr m_vframe;   ///< quadro em memória (entrada do encoder ou origem do upload)
    FramePtr m_hwFrame;  ///< superfície de GPU (VA-API)
    AVBufferRef *m_hwDevice = nullptr;
    AVBufferRef *m_hwFrames = nullptr;
    bool m_hwDirect = false; ///< encoder de hardware que aceita quadros em memória (NVENC, QSV...)
    bool m_alpha = false;
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

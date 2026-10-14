// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once

extern "C" {
#include <libavcodec/avcodec.h>
#include <libavformat/avformat.h>
#include <libavutil/avutil.h>
#include <libswresample/swresample.h>
#include <libswscale/swscale.h>
}

#include <QString>

#include <memory>

namespace clv::media {

struct FormatCtxDeleter {
    void operator()(AVFormatContext *c) const { avformat_close_input(&c); }
};
struct CodecCtxDeleter {
    void operator()(AVCodecContext *c) const { avcodec_free_context(&c); }
};
struct FrameDeleter {
    void operator()(AVFrame *f) const { av_frame_free(&f); }
};
struct PacketDeleter {
    void operator()(AVPacket *p) const { av_packet_free(&p); }
};
struct SwsDeleter {
    void operator()(SwsContext *c) const { sws_freeContext(c); }
};
struct SwrDeleter {
    void operator()(SwrContext *c) const { swr_free(&c); }
};

using FormatCtxPtr = std::unique_ptr<AVFormatContext, FormatCtxDeleter>;
using CodecCtxPtr = std::unique_ptr<AVCodecContext, CodecCtxDeleter>;
using FramePtr = std::unique_ptr<AVFrame, FrameDeleter>;
using PacketPtr = std::unique_ptr<AVPacket, PacketDeleter>;
using SwsPtr = std::unique_ptr<SwsContext, SwsDeleter>;
using SwrPtr = std::unique_ptr<SwrContext, SwrDeleter>;

inline QString avError(int err)
{
    char buf[AV_ERROR_MAX_STRING_SIZE] = {};
    av_strerror(err, buf, sizeof buf);
    return QString::fromUtf8(buf);
}

/** Mostra só erros do FFmpeg no terminal (sem o relatório do libx264 etc.). CLIVCUT_FFMPEG_LOG=1 mantém tudo. */
void quietFFmpegLogs();

/** Abre um arquivo e lê as informações de streams. */
FormatCtxPtr openInput(const QString &path, QString *error = nullptr);

/** Abre o decodificador para um stream (threads automáticas). */
CodecCtxPtr openDecoder(AVStream *stream, QString *error = nullptr);

/**
 * Abre o decodificador com aceleração por hardware quando possível
 * (VA-API/CUDA no Linux, D3D11VA/DXVA2 no Windows, VideoToolbox no macOS).
 * `hwFormat` recebe o formato de pixel de hardware, ou AV_PIX_FMT_NONE se
 * caiu para software. CLIVCUT_HWDEC=0 desliga.
 */
CodecCtxPtr openVideoDecoder(AVStream *stream, bool allowHardware, AVPixelFormat *hwFormat, QString *error = nullptr);

/** Nome do dispositivo de hardware em uso ("vaapi"...) ou vazio. */
QString hardwareDecoderName(AVPixelFormat hwFormat);

/** Converte um timestamp do stream para µs. */
inline std::int64_t streamToMicros(std::int64_t ts, AVRational tb)
{
    return av_rescale_q(ts, tb, AVRational{1, 1'000'000});
}

inline std::int64_t microsToStream(std::int64_t us, AVRational tb)
{
    return av_rescale_q(us, AVRational{1, 1'000'000}, tb);
}

/** Codecs "de imagem" (um único quadro) que não devem ser tratados como vídeo. */
bool isStillImageCodec(AVCodecID id);

} // namespace clv::media

// SPDX-License-Identifier: GPL-3.0-or-later
#include "FFmpegUtil.h"

extern "C" {
#include <libavutil/hwcontext.h>
#include <libavutil/pixdesc.h>
}

#include <QCoreApplication>
#include <QMutex>
#include <QThread>

#include <map>

namespace clv::media {

void quietFFmpegLogs()
{
    if (qEnvironmentVariable("CLIVCUT_FFMPEG_LOG") != QLatin1String("1"))
        av_log_set_level(AV_LOG_ERROR);
}

FormatCtxPtr openInput(const QString &path, QString *error)
{
    AVFormatContext *raw = nullptr;
    int err = avformat_open_input(&raw, path.toUtf8().constData(), nullptr, nullptr);
    if (err < 0) {
        if (error)
            *error = QCoreApplication::translate("Media", "Could not open %1: %2").arg(path, avError(err));
        return {};
    }
    FormatCtxPtr ctx(raw);
    err = avformat_find_stream_info(ctx.get(), nullptr);
    if (err < 0) {
        if (error)
            *error = QCoreApplication::translate("Media", "Failed to read streams: %1").arg(avError(err));
        return {};
    }
    return ctx;
}

CodecCtxPtr openDecoder(AVStream *stream, QString *error)
{
    const AVCodec *codec = avcodec_find_decoder(stream->codecpar->codec_id);
    if (!codec) {
        if (error)
            *error = QCoreApplication::translate("Media", "Unsupported codec: %1").arg(QString::fromLatin1(avcodec_get_name(stream->codecpar->codec_id)));
        return {};
    }
    CodecCtxPtr ctx(avcodec_alloc_context3(codec));
    avcodec_parameters_to_context(ctx.get(), stream->codecpar);
    ctx->pkt_timebase = stream->time_base;
    ctx->thread_count = std::min(QThread::idealThreadCount(), 8);
    ctx->thread_type = FF_THREAD_FRAME | FF_THREAD_SLICE;
    const int err = avcodec_open2(ctx.get(), codec, nullptr);
    if (err < 0) {
        if (error)
            *error = QCoreApplication::translate("Media", "Failed to open decoder: %1").arg(avError(err));
        return {};
    }
    return ctx;
}

namespace {

std::vector<AVHWDeviceType> preferredDeviceTypes()
{
#if defined(Q_OS_WIN)
    return {AV_HWDEVICE_TYPE_D3D11VA, AV_HWDEVICE_TYPE_DXVA2, AV_HWDEVICE_TYPE_CUDA};
#elif defined(Q_OS_MACOS)
    return {AV_HWDEVICE_TYPE_VIDEOTOOLBOX};
#else
    return {AV_HWDEVICE_TYPE_VAAPI, AV_HWDEVICE_TYPE_CUDA};
#endif
}

/** Um dispositivo por tipo, compartilhado por todos os decodificadores (nulo se falhou). */
AVBufferRef *sharedDevice(AVHWDeviceType type)
{
    static QMutex mutex;
    static std::map<AVHWDeviceType, AVBufferRef *> devices;
    QMutexLocker lock(&mutex);
    auto it = devices.find(type);
    if (it == devices.end()) {
        AVBufferRef *ref = nullptr;
        if (av_hwdevice_ctx_create(&ref, type, nullptr, nullptr, 0) < 0)
            ref = nullptr;
        it = devices.emplace(type, ref).first;
    }
    return it->second;
}

AVPixelFormat pickFormat(AVCodecContext *ctx, const AVPixelFormat *fmts)
{
    const auto want = static_cast<AVPixelFormat>(reinterpret_cast<intptr_t>(ctx->opaque));
    for (const AVPixelFormat *p = fmts; *p != AV_PIX_FMT_NONE; ++p)
        if (*p == want)
            return *p;
    // Sem suporte do hardware para este stream: primeiro formato de software.
    for (const AVPixelFormat *p = fmts; *p != AV_PIX_FMT_NONE; ++p)
        if (!(av_pix_fmt_desc_get(*p)->flags & AV_PIX_FMT_FLAG_HWACCEL))
            return *p;
    return fmts[0];
}

} // namespace

CodecCtxPtr openVideoDecoder(AVStream *stream, bool allowHardware, AVPixelFormat *hwFormat, QString *error)
{
    *hwFormat = AV_PIX_FMT_NONE;
    const AVCodec *codec = avcodec_find_decoder(stream->codecpar->codec_id);
    if (!codec || !allowHardware || qEnvironmentVariable("CLIVCUT_HWDEC") == QLatin1String("0"))
        return openDecoder(stream, error);

    for (AVHWDeviceType type : preferredDeviceTypes()) {
        AVPixelFormat fmt = AV_PIX_FMT_NONE;
        for (int i = 0;; ++i) {
            const AVCodecHWConfig *cfg = avcodec_get_hw_config(codec, i);
            if (!cfg)
                break;
            if ((cfg->methods & AV_CODEC_HW_CONFIG_METHOD_HW_DEVICE_CTX) && cfg->device_type == type) {
                fmt = cfg->pix_fmt;
                break;
            }
        }
        if (fmt == AV_PIX_FMT_NONE)
            continue;
        AVBufferRef *device = sharedDevice(type);
        if (!device)
            continue;
        CodecCtxPtr ctx(avcodec_alloc_context3(codec));
        avcodec_parameters_to_context(ctx.get(), stream->codecpar);
        ctx->pkt_timebase = stream->time_base;
        ctx->hw_device_ctx = av_buffer_ref(device);
        ctx->opaque = reinterpret_cast<void *>(static_cast<intptr_t>(fmt));
        ctx->get_format = pickFormat;
        ctx->extra_hw_frames = 4; // quadros retidos (atual + seguinte) além do pool do codec
        if (avcodec_open2(ctx.get(), codec, nullptr) == 0) {
            *hwFormat = fmt;
            return ctx;
        }
    }
    return openDecoder(stream, error);
}

QString hardwareDecoderName(AVPixelFormat hwFormat)
{
    if (hwFormat == AV_PIX_FMT_NONE)
        return {};
    const char *name = av_get_pix_fmt_name(hwFormat);
    return name ? QString::fromLatin1(name) : QString();
}

bool isStillImageCodec(AVCodecID id)
{
    switch (id) {
    case AV_CODEC_ID_PNG:
    case AV_CODEC_ID_MJPEG:
    case AV_CODEC_ID_BMP:
    case AV_CODEC_ID_TIFF:
    case AV_CODEC_ID_WEBP:
    case AV_CODEC_ID_JPEGXL:
    case AV_CODEC_ID_GIF:
        return true;
    default:
        return false;
    }
}

} // namespace clv::media

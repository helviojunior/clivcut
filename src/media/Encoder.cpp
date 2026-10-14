// SPDX-License-Identifier: GPL-3.0-or-later
#include "Encoder.h"

#include "AudioDecoder.h"

extern "C" {
#include <libavutil/channel_layout.h>
#include <libavutil/opt.h>
}

#include <QThread>

#include <cstring>

namespace clv::media {

namespace {

struct CodecChoice {
    const char *encoder;
    AVPixelFormat pixfmt;
};

std::vector<CodecChoice> candidates(VideoCodec c)
{
    switch (c) {
    case VideoCodec::H264: return {{"libx264", AV_PIX_FMT_YUV420P}, {"libopenh264", AV_PIX_FMT_YUV420P}};
    case VideoCodec::HEVC: return {{"libx265", AV_PIX_FMT_YUV420P}};
    case VideoCodec::AV1: return {{"libsvtav1", AV_PIX_FMT_YUV420P}, {"libaom-av1", AV_PIX_FMT_YUV420P}};
    case VideoCodec::ProRes: return {{"prores_ks", AV_PIX_FMT_YUV422P10LE}};
    }
    return {};
}

} // namespace

QStringList availableVideoCodecs()
{
    QStringList out;
    const std::pair<VideoCodec, const char *> all[] = {
        {VideoCodec::H264, "h264"}, {VideoCodec::HEVC, "hevc"}, {VideoCodec::AV1, "av1"}, {VideoCodec::ProRes, "prores"}};
    for (const auto &[c, name] : all)
        for (const auto &cand : candidates(c))
            if (avcodec_find_encoder_by_name(cand.encoder)) {
                out << QString::fromLatin1(name);
                break;
            }
    return out;
}

Encoder::~Encoder()
{
    if (m_out && m_headerWritten && !m_finished)
        finish();
    if (m_out) {
        if (!(m_out->oformat->flags & AVFMT_NOFILE) && m_out->pb)
            avio_closep(&m_out->pb);
        avformat_free_context(m_out);
    }
}

bool Encoder::open(const QString &path, const EncodeSettings &s, QString *error)
{
    auto fail = [&](const QString &msg) {
        if (error)
            *error = msg;
        return false;
    };
    AVFormatContext *oc = nullptr;
    int err = avformat_alloc_output_context2(&oc, nullptr, nullptr, path.toUtf8().constData());
    if (err < 0 || !oc)
        return fail(QStringLiteral("Formato de saída não suportado: %1").arg(avError(err)));
    m_out = oc;

    // --- vídeo ---
    const AVCodec *vcodec = nullptr;
    AVPixelFormat pixfmt = AV_PIX_FMT_YUV420P;
    for (const auto &cand : candidates(s.codec)) {
        if ((vcodec = avcodec_find_encoder_by_name(cand.encoder))) {
            pixfmt = cand.pixfmt;
            break;
        }
    }
    if (!vcodec)
        return fail(QStringLiteral("Nenhum encoder disponível para o codec escolhido neste FFmpeg"));
    m_codecName = QString::fromLatin1(vcodec->name);
    m_vst = avformat_new_stream(oc, nullptr);
    m_venc.reset(avcodec_alloc_context3(vcodec));
    AVCodecContext *v = m_venc.get();
    v->width = s.width & ~1;
    v->height = s.height & ~1;
    const AVRational fr = av_d2q(s.fps, 100000);
    v->framerate = fr;
    v->time_base = av_inv_q(fr);
    v->pix_fmt = pixfmt;
    v->gop_size = s.gopFrames > 0 ? s.gopFrames : static_cast<int>(s.fps * 2);
    v->color_primaries = AVCOL_PRI_BT709;
    v->color_trc = AVCOL_TRC_BT709;
    v->colorspace = AVCOL_SPC_BT709;
    v->color_range = AVCOL_RANGE_MPEG;
    v->thread_count = std::min(QThread::idealThreadCount(), 16);
    if (oc->oformat->flags & AVFMT_GLOBALHEADER)
        v->flags |= AV_CODEC_FLAG_GLOBAL_HEADER;
    if (s.bitrateKbps > 0) {
        v->bit_rate = static_cast<int64_t>(s.bitrateKbps) * 1000;
    } else if (m_codecName == QLatin1String("libx264") || m_codecName == QLatin1String("libx265")) {
        av_opt_set_int(v->priv_data, "crf", s.crf, 0);
    } else if (m_codecName == QLatin1String("libsvtav1")) {
        av_opt_set_int(v->priv_data, "crf", s.crf + 10, 0);
    } else if (m_codecName == QLatin1String("libaom-av1")) {
        av_opt_set_int(v->priv_data, "crf", s.crf + 10, 0);
        v->bit_rate = 0;
    } else if (m_codecName == QLatin1String("prores_ks")) {
        av_opt_set_int(v->priv_data, "profile", 3, 0); // HQ
    } else {
        v->bit_rate = static_cast<int64_t>(v->width) * v->height * s.fps * 0.1;
    }
    if (m_codecName.startsWith(QLatin1String("libx26")))
        av_opt_set(v->priv_data, "preset", s.preset.toLatin1().constData(), 0);
    if (m_codecName == QLatin1String("libx265"))
        av_opt_set(v->priv_data, "tag", "hvc1", 0);
    err = avcodec_open2(v, vcodec, nullptr);
    if (err < 0)
        return fail(QStringLiteral("Falha ao abrir o encoder %1: %2").arg(m_codecName, avError(err)));
    avcodec_parameters_from_context(m_vst->codecpar, v);
    m_vst->time_base = v->time_base;
    m_vst->avg_frame_rate = fr;
    if (m_codecName == QLatin1String("libx265"))
        m_vst->codecpar->codec_tag = MKTAG('h', 'v', 'c', '1');

    m_vframe.reset(av_frame_alloc());
    m_vframe->format = v->pix_fmt;
    m_vframe->width = v->width;
    m_vframe->height = v->height;
    av_frame_get_buffer(m_vframe.get(), 0);

    // --- áudio ---
    if (s.audio) {
        const AVCodec *acodec = avcodec_find_encoder(AV_CODEC_ID_AAC);
        if (oc->oformat == av_guess_format("matroska", nullptr, nullptr) && avcodec_find_encoder_by_name("libopus"))
            acodec = avcodec_find_encoder_by_name("libopus");
        if (acodec) {
            m_ast = avformat_new_stream(oc, nullptr);
            m_aenc.reset(avcodec_alloc_context3(acodec));
            AVCodecContext *a = m_aenc.get();
            a->sample_rate = kSampleRate;
            a->sample_fmt = AV_SAMPLE_FMT_FLTP;
            av_channel_layout_default(&a->ch_layout, 2);
            a->bit_rate = static_cast<int64_t>(s.audioBitrateKbps) * 1000;
            a->time_base = {1, kSampleRate};
            if (oc->oformat->flags & AVFMT_GLOBALHEADER)
                a->flags |= AV_CODEC_FLAG_GLOBAL_HEADER;
            err = avcodec_open2(a, acodec, nullptr);
            if (err < 0)
                return fail(QStringLiteral("Falha ao abrir o encoder de áudio: %1").arg(avError(err)));
            avcodec_parameters_from_context(m_ast->codecpar, a);
            m_ast->time_base = a->time_base;
            m_aframe.reset(av_frame_alloc());
            m_aframe->format = a->sample_fmt;
            av_channel_layout_copy(&m_aframe->ch_layout, &a->ch_layout);
            m_aframe->sample_rate = a->sample_rate;
            m_aframe->nb_samples = a->frame_size > 0 ? a->frame_size : 1024;
            av_frame_get_buffer(m_aframe.get(), 0);
        }
    }

    if (!(oc->oformat->flags & AVFMT_NOFILE)) {
        err = avio_open(&oc->pb, path.toUtf8().constData(), AVIO_FLAG_WRITE);
        if (err < 0)
            return fail(QStringLiteral("Não foi possível criar %1: %2").arg(path, avError(err)));
    }
    AVDictionary *opts = nullptr;
    if (path.endsWith(QLatin1String(".mp4"), Qt::CaseInsensitive) || path.endsWith(QLatin1String(".mov"), Qt::CaseInsensitive))
        av_dict_set(&opts, "movflags", "+faststart", 0);
    err = avformat_write_header(oc, &opts);
    av_dict_free(&opts);
    if (err < 0)
        return fail(QStringLiteral("Falha ao escrever cabeçalho: %1").arg(avError(err)));
    m_headerWritten = true;
    m_pkt.reset(av_packet_alloc());
    return true;
}

bool Encoder::encode(AVCodecContext *ctx, AVStream *st, AVFrame *f)
{
    int err = avcodec_send_frame(ctx, f);
    if (err < 0 && err != AVERROR_EOF)
        return false;
    for (;;) {
        err = avcodec_receive_packet(ctx, m_pkt.get());
        if (err == AVERROR(EAGAIN) || err == AVERROR_EOF)
            return true;
        if (err < 0)
            return false;
        av_packet_rescale_ts(m_pkt.get(), ctx->time_base, st->time_base);
        m_pkt->stream_index = st->index;
        if (av_interleaved_write_frame(m_out, m_pkt.get()) < 0)
            return false;
    }
}

bool Encoder::writeVideo(const QImage &img)
{
    AVCodecContext *v = m_venc.get();
    const QImage src = img.format() == QImage::Format_RGBA8888 || img.format() == QImage::Format_RGBA8888_Premultiplied
                           ? img
                           : img.convertToFormat(QImage::Format_RGBA8888);
    if (!m_sws) {
        m_sws.reset(sws_getContext(src.width(), src.height(), AV_PIX_FMT_RGBA, v->width, v->height, v->pix_fmt,
                                   SWS_BICUBIC, nullptr, nullptr, nullptr));
        sws_setColorspaceDetails(m_sws.get(), sws_getCoefficients(SWS_CS_DEFAULT), 1, sws_getCoefficients(SWS_CS_ITU709),
                                 0, 0, 1 << 16, 1 << 16);
    }
    if (av_frame_make_writable(m_vframe.get()) < 0)
        return false;
    const uint8_t *srcData[4] = {src.constBits(), nullptr, nullptr, nullptr};
    const int srcStride[4] = {static_cast<int>(src.bytesPerLine()), 0, 0, 0};
    sws_scale(m_sws.get(), srcData, srcStride, 0, src.height(), m_vframe->data, m_vframe->linesize);
    m_vframe->pts = m_videoPts++;
    return encode(v, m_vst, m_vframe.get());
}

bool Encoder::flushAudioFifo(bool final)
{
    if (!m_aenc)
        return true;
    AVCodecContext *a = m_aenc.get();
    const int n = m_aframe->nb_samples;
    while (m_audioFifo.size() >= static_cast<size_t>(n * kChannels) || (final && !m_audioFifo.empty())) {
        const int take = std::min<int>(n, static_cast<int>(m_audioFifo.size() / kChannels));
        if (av_frame_make_writable(m_aframe.get()) < 0)
            return false;
        auto *l = reinterpret_cast<float *>(m_aframe->data[0]);
        auto *r = reinterpret_cast<float *>(m_aframe->data[1]);
        for (int i = 0; i < n; ++i) {
            l[i] = i < take ? m_audioFifo[i * 2] : 0.f;
            r[i] = i < take ? m_audioFifo[i * 2 + 1] : 0.f;
        }
        m_audioFifo.erase(m_audioFifo.begin(), m_audioFifo.begin() + take * kChannels);
        m_aframe->pts = m_audioPts;
        m_audioPts += n;
        if (!encode(a, m_ast, m_aframe.get()))
            return false;
    }
    return true;
}

bool Encoder::writeAudio(const float *interleaved, int frames)
{
    if (!m_aenc)
        return true;
    m_audioFifo.insert(m_audioFifo.end(), interleaved, interleaved + static_cast<size_t>(frames) * kChannels);
    return flushAudioFifo(false);
}

bool Encoder::finish(QString *error)
{
    if (m_finished || !m_headerWritten)
        return m_finished;
    m_finished = true;
    bool ok = flushAudioFifo(true);
    ok = encode(m_venc.get(), m_vst, nullptr) && ok;
    if (m_aenc)
        ok = encode(m_aenc.get(), m_ast, nullptr) && ok;
    const int err = av_write_trailer(m_out);
    if (err < 0 || !ok) {
        if (error)
            *error = QStringLiteral("Falha ao finalizar o arquivo: %1").arg(avError(err));
        return false;
    }
    return true;
}

} // namespace clv::media

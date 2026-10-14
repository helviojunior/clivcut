// SPDX-License-Identifier: GPL-3.0-or-later
#include "MediaProbe.h"

#include "AudioDecoder.h"
#include "FFmpegUtil.h"
#include "VideoDecoder.h"

extern "C" {
#include <libavutil/display.h>
}

#include <QFileInfo>
#include <QImageReader>

#include <algorithm>
#include <cmath>

namespace clv::media {

bool probe(const QString &path, MediaAsset &out, QString *error)
{
    out.path = path;
    out.name = QFileInfo(path).fileName();

    // Imagem estática (inclui orientação EXIF).
    QImageReader reader(path);
    reader.setAutoTransform(true);
    if (!reader.format().isEmpty() && reader.format() != "gif" && reader.imageCount() <= 1) {
        const QSize sz = reader.size();
        if (sz.isValid()) {
            out.kind = MediaKind::Image;
            out.width = sz.width();
            out.height = sz.height();
            if (reader.transformation() & QImageIOHandler::TransformationRotate90)
                std::swap(out.width, out.height);
            out.duration = 0;
            out.hasVideo = true;
            out.hasAudio = false;
            return true;
        }
    }

    auto fmt = openInput(path, error);
    if (!fmt)
        return false;
    const int v = av_find_best_stream(fmt.get(), AVMEDIA_TYPE_VIDEO, -1, -1, nullptr, 0);
    const int a = av_find_best_stream(fmt.get(), AVMEDIA_TYPE_AUDIO, -1, -1, nullptr, 0);
    const bool coverArt = v >= 0 && (fmt->streams[v]->disposition & AV_DISPOSITION_ATTACHED_PIC);
    out.hasVideo = v >= 0 && !coverArt;
    out.hasAudio = a >= 0;
    if (!out.hasVideo && !out.hasAudio) {
        if (error)
            *error = QStringLiteral("Arquivo sem vídeo ou áudio reconhecível");
        return false;
    }
    out.duration = fmt->duration > 0 ? fmt->duration : 0;
    if (out.hasVideo) {
        AVStream *st = fmt->streams[v];
        out.width = st->codecpar->width;
        out.height = st->codecpar->height;
        const AVPacketSideData *sd = av_packet_side_data_get(st->codecpar->coded_side_data, st->codecpar->nb_coded_side_data,
                                                             AV_PKT_DATA_DISPLAYMATRIX);
        if (sd && sd->size >= 36) {
            const double rot = av_display_rotation_get(reinterpret_cast<const int32_t *>(sd->data));
            if (!std::isnan(rot) && std::lround(std::abs(rot)) % 180 == 90)
                std::swap(out.width, out.height);
        }
        const AVRational fr = st->avg_frame_rate.num > 0 ? st->avg_frame_rate : st->r_frame_rate;
        out.fps = fr.den > 0 ? static_cast<double>(fr.num) / fr.den : 0;
        if (st->duration != AV_NOPTS_VALUE)
            out.duration = streamToMicros(st->duration, st->time_base);
        // GIF animado e vídeos de 1 quadro se comportam como imagem se não tiverem duração.
        const bool still = isStillImageCodec(st->codecpar->codec_id) && st->nb_frames <= 1 && out.duration <= 100'000;
        out.kind = still ? MediaKind::Image : MediaKind::Video;
        if (still)
            out.duration = 0;
    } else {
        out.kind = MediaKind::Audio;
        AVStream *st = fmt->streams[a];
        if (st->duration != AV_NOPTS_VALUE)
            out.duration = streamToMicros(st->duration, st->time_base);
    }
    return true;
}

QImage thumbnail(const QString &path, Micros t, int maxWidth)
{
    VideoDecoder dec;
    // Miniaturas decodificam poucos quadros: software evita o custo de iniciar a GPU.
    if (!dec.open(path, QSize(maxWidth, maxWidth * 4), nullptr, false))
        return {};
    return dec.frameAt(t);
}

std::vector<float> waveform(const QString &path, int buckets)
{
    std::vector<float> peaks(std::max(1, buckets), 0.f);
    AudioDecoder dec;
    if (!dec.open(path) || dec.duration() <= 0)
        return peaks;
    const std::int64_t totalFrames = av_rescale(dec.duration(), kSampleRate, kSecond);
    const std::int64_t perBucket = std::max<std::int64_t>(1, totalFrames / buckets);
    constexpr int kChunk = 8192;
    std::vector<float> buf(kChunk * kChannels);
    std::int64_t pos = 0;
    while (pos < totalFrames) {
        const Micros t = av_rescale(pos, kSecond, kSampleRate);
        const int got = dec.read(t, buf.data(), kChunk);
        if (got <= 0)
            break;
        for (int i = 0; i < got; ++i) {
            const auto b = std::min<std::int64_t>((pos + i) / perBucket, buckets - 1);
            const float s = std::max(std::abs(buf[i * 2]), std::abs(buf[i * 2 + 1]));
            peaks[b] = std::max(peaks[b], s);
        }
        pos += got;
    }
    return peaks;
}

} // namespace clv::media

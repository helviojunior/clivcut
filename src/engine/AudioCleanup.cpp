// SPDX-License-Identifier: GPL-3.0-or-later
#include "AudioCleanup.h"

#include "Proxy.h"
#include "media/AudioDecoder.h"

extern "C" {
#include <libavfilter/avfilter.h>
#include <libavfilter/buffersink.h>
#include <libavfilter/buffersrc.h>
#include <libavutil/channel_layout.h>
#include <libavutil/frame.h>
#include <libavutil/opt.h>
}

#include <QCryptographicHash>
#include <QDateTime>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QHash>
#include <QMutex>

#include <algorithm>
#include <cmath>
#include <cstring>
#include <vector>

namespace clv::engine {

using media::kChannels;
using media::kSampleRate;

namespace {

QMutex g_mutex;
QHash<QString, QString> g_clean;

QString key(const QString &path, double strength)
{
    return path + QLatin1Char('|') + QString::number(quantizedDenoise(strength), 'f', 1);
}

/** Cabeçalho WAV (float 32 bits, estéreo, 48 kHz) para `frames` quadros. */
QByteArray wavHeader(std::int64_t frames)
{
    const quint32 dataBytes = static_cast<quint32>(std::min<std::int64_t>(frames * kChannels * 4, 0xFFFFFFF0LL));
    QByteArray h;
    auto u32 = [&](quint32 v) { h.append(reinterpret_cast<const char *>(&v), 4); };
    auto u16 = [&](quint16 v) { h.append(reinterpret_cast<const char *>(&v), 2); };
    h.append("RIFF");
    u32(36 + dataBytes);
    h.append("WAVEfmt ");
    u32(16);
    u16(3); // IEEE float
    u16(kChannels);
    u32(kSampleRate);
    u32(kSampleRate * kChannels * 4);
    u16(kChannels * 4);
    u16(32);
    h.append("data");
    u32(dataBytes);
    return h;
}

struct Graph {
    AVFilterGraph *graph = nullptr;
    AVFilterContext *src = nullptr;
    AVFilterContext *sink = nullptr;
    ~Graph() { avfilter_graph_free(&graph); }

    bool init(double strength, int noiseFloorDb)
    {
        graph = avfilter_graph_alloc();
        const QByteArray args =
            QStringLiteral("sample_rate=%1:sample_fmt=flt:channel_layout=stereo:time_base=1/%1").arg(kSampleRate).toLatin1();
        if (avfilter_graph_create_filter(&src, avfilter_get_by_name("abuffer"), "in", args.constData(), nullptr, graph) < 0)
            return false;
        if (avfilter_graph_create_filter(&sink, avfilter_get_by_name("abuffersink"), "out", nullptr, nullptr, graph) < 0)
            return false;
        // Passa-altas tira ronco/vento; afftdn com o nível de ruído medido no próprio arquivo
        // (o rastreamento automático do filtro quase não reduz quando o nível inicial está errado).
        // Inteiros: o avaliador do FFmpeg usa strtod, que depende do locale (vírgula em pt-BR).
        const int nr = static_cast<int>(std::lround(6 + 34 * std::clamp(strength, 0.0, 1.0)));
        const QByteArray chain =
            QStringLiteral("highpass=f=70,afftdn=nr=%1:nf=%2,aformat=sample_fmts=flt:channel_layouts=stereo")
                .arg(nr)
                .arg(noiseFloorDb)
                .toLatin1();
        AVFilterInOut *outputs = avfilter_inout_alloc();
        AVFilterInOut *inputs = avfilter_inout_alloc();
        outputs->name = av_strdup("in");
        outputs->filter_ctx = src;
        inputs->name = av_strdup("out");
        inputs->filter_ctx = sink;
        const int rc = avfilter_graph_parse_ptr(graph, chain.constData(), &inputs, &outputs, nullptr);
        avfilter_inout_free(&inputs);
        avfilter_inout_free(&outputs);
        return rc >= 0 && avfilter_graph_config(graph, nullptr) >= 0;
    }
};

/**
 * Estima o nível de ruído de fundo (dBFS): percentil 10 da energia em janelas
 * de 50 ms, ou seja, os trechos mais silenciosos (pausas da fala).
 */
int estimateNoiseFloor(media::AudioDecoder &dec, std::int64_t total, const std::function<bool(double)> &progress)
{
    constexpr int kWin = kSampleRate / 20;
    std::vector<float> buf(static_cast<size_t>(kWin) * kChannels);
    std::vector<double> levels;
    for (std::int64_t pos = 0; pos + kWin <= total; pos += kWin) {
        dec.read(av_rescale(pos, kSecond, kSampleRate), buf.data(), kWin);
        double acc = 0;
        for (float x : buf)
            acc += static_cast<double>(x) * x;
        const double ms = acc / buf.size();
        if (ms > 1e-12) // silêncio digital (zeros) não conta como ruído
            levels.push_back(10 * std::log10(ms));
        if (progress && (pos / kWin) % 200 == 0 && !progress(0.2 * pos / total))
            return 0;
    }
    if (levels.empty())
        return -80;
    std::nth_element(levels.begin(), levels.begin() + levels.size() / 10, levels.end());
    return static_cast<int>(std::lround(std::clamp(levels[levels.size() / 10], -80.0, -20.0)));
}

/** Empurra `frames` quadros (estéreo intercalado) no filtro e recolhe a saída. */
void runThrough(Graph &g, const float *data, int frames, std::int64_t pts, std::vector<float> &out)
{
    AVFrame *in = av_frame_alloc();
    AVFrame *res = av_frame_alloc();
    in->format = AV_SAMPLE_FMT_FLT;
    in->sample_rate = kSampleRate;
    av_channel_layout_default(&in->ch_layout, kChannels);
    in->nb_samples = frames;
    in->pts = pts;
    if (av_frame_get_buffer(in, 0) >= 0) {
        std::memcpy(in->data[0], data, sizeof(float) * frames * kChannels);
        if (av_buffersrc_add_frame(g.src, in) >= 0)
            while (av_buffersink_get_frame(g.sink, res) >= 0) {
                const auto *f = reinterpret_cast<const float *>(res->data[0]);
                out.insert(out.end(), f, f + res->nb_samples * kChannels);
                av_frame_unref(res);
            }
    }
    av_frame_free(&in);
    av_frame_free(&res);
}

/**
 * Latência do filtro (em quadros): o afftdn atrasa o conteúdo sem corrigir o
 * pts. Mede com um trecho de ruído forte e correlação cruzada.
 */
int measureLatency(double strength, int floorDb)
{
    Graph g;
    if (!g.init(strength, floorDb))
        return 0;
    constexpr int kLen = kSampleRate; // 1 s
    std::vector<float> probe(static_cast<size_t>(kLen) * kChannels);
    std::uint32_t seed = 12345;
    for (size_t i = 0; i < probe.size(); i += 2) {
        seed = seed * 1664525u + 1013904223u;
        const float v = (static_cast<float>(seed >> 8) / 16777216.0f - 0.5f) * 0.6f;
        probe[i] = probe[i + 1] = v;
    }
    std::vector<float> out;
    runThrough(g, probe.data(), kLen, 0, out);
    std::vector<float> zeros(static_cast<size_t>(kLen / 4) * kChannels, 0.f);
    runThrough(g, zeros.data(), kLen / 4, kLen, out); // empurra o que ficou retido
    const int n = static_cast<int>(std::min(out.size(), probe.size()) / kChannels);
    int bestLag = 0;
    double best = -1;
    for (int lag = 0; lag <= 8192 && lag < n / 2; ++lag) {
        double acc = 0;
        for (int i = n / 4; i < n / 2; ++i)
            acc += static_cast<double>(probe[i * 2]) * out[(i + lag) * 2];
        if (acc > best) {
            best = acc;
            bestLag = lag;
        }
    }
    return bestLag;
}

} // namespace

double quantizedDenoise(double strength)
{
    return std::clamp(std::round(strength * 10) / 10, 0.0, 1.0);
}

QString denoisedPathFor(const QString &mediaPath, double strength)
{
    const QFileInfo fi(mediaPath);
    const QByteArray k = fi.absoluteFilePath().toUtf8() + '|' + QByteArray::number(fi.size()) + '|' +
                         QByteArray::number(fi.lastModified().toMSecsSinceEpoch()) + '|' +
                         QByteArray::number(quantizedDenoise(strength), 'f', 1);
    const QString hash = QString::fromLatin1(QCryptographicHash::hash(k, QCryptographicHash::Sha1).toHex().left(16));
    const QString dir = QDir(QFileInfo(proxyCacheDir()).absolutePath()).filePath(QStringLiteral("audio"));
    return QDir(dir).filePath(QStringLiteral("%1_nr%2_%3.wav")
                                  .arg(fi.completeBaseName().left(40))
                                  .arg(static_cast<int>(quantizedDenoise(strength) * 10))
                                  .arg(hash));
}

bool generateDenoised(const QString &mediaPath, const QString &outPath, double strength,
                      const std::function<bool(double)> &progress, QString *error)
{
    auto fail = [&](const QString &m) {
        if (error)
            *error = m;
        return false;
    };
    media::AudioDecoder dec;
    if (!dec.open(mediaPath, error))
        return false;
    const std::int64_t total = av_rescale(dec.duration(), kSampleRate, kSecond);
    if (total <= 0)
        return fail(QStringLiteral("Áudio sem duração"));
    // 1ª passada (20% do progresso): nível de ruído. 2ª: filtragem.
    bool cancelled = false;
    const int floorDb = estimateNoiseFloor(dec, total, [&](double f) {
        cancelled = progress && !progress(f);
        return !cancelled;
    });
    if (cancelled)
        return fail(QStringLiteral("Redução de ruído cancelada"));
    const int latency = measureLatency(strength, floorDb);
    Graph g;
    if (!g.init(strength, floorDb))
        return fail(QStringLiteral("Filtro de redução de ruído indisponível neste FFmpeg"));

    QDir().mkpath(QFileInfo(outPath).absolutePath());
    const QString tmp = outPath + QStringLiteral(".part");
    QFile out(tmp);
    if (!out.open(QIODevice::ReadWrite | QIODevice::Truncate))
        return fail(out.errorString());
    const QByteArray header = wavHeader(total);
    out.write(header);
    // Silêncio em todo o arquivo; cada bloco filtrado é gravado na posição do seu pts.
    {
        QByteArray zeros(1 << 20, 0);
        std::int64_t remaining = total * kChannels * 4;
        while (remaining > 0) {
            const qint64 n = std::min<std::int64_t>(remaining, zeros.size());
            out.write(zeros.constData(), n);
            remaining -= n;
        }
    }

    AVFrame *in = av_frame_alloc();
    AVFrame *filtered = av_frame_alloc();
    constexpr int kChunk = 4096;
    std::int64_t fed = 0;
    bool ok = true;
    auto drain = [&]() {
        while (av_buffersink_get_frame(g.sink, filtered) >= 0) {
            // Compensa a latência: o conteúdo deste bloco pertence a pts - latency.
            std::int64_t pos = (filtered->pts == AV_NOPTS_VALUE ? 0 : filtered->pts) - latency;
            std::int64_t skip = 0;
            if (pos < 0) {
                skip = -pos;
                pos = 0;
            }
            const std::int64_t n = std::min<std::int64_t>(filtered->nb_samples - skip, total - pos);
            if (n > 0) {
                out.seek(header.size() + pos * kChannels * 4);
                out.write(reinterpret_cast<const char *>(filtered->data[0]) + skip * kChannels * 4, n * kChannels * 4);
            }
            av_frame_unref(filtered);
        }
    };
    while (fed < total && ok) {
        const int n = static_cast<int>(std::min<std::int64_t>(kChunk, total - fed));
        in->format = AV_SAMPLE_FMT_FLT;
        in->sample_rate = kSampleRate;
        av_channel_layout_default(&in->ch_layout, kChannels);
        in->nb_samples = n;
        in->pts = fed;
        if (av_frame_get_buffer(in, 0) < 0) {
            ok = false;
            break;
        }
        dec.read(av_rescale(fed, kSecond, kSampleRate), reinterpret_cast<float *>(in->data[0]), n);
        if (av_buffersrc_add_frame(g.src, in) < 0)
            ok = false;
        av_frame_unref(in);
        drain();
        fed += n;
        if (progress && !progress(0.2 + 0.8 * static_cast<double>(fed) / total)) {
            ok = false;
            if (error)
                *error = QStringLiteral("Redução de ruído cancelada");
        }
    }
    if (ok && latency > 0) {
        // Cauda: silêncio do tamanho da latência para o filtro devolver o final do áudio.
        in->format = AV_SAMPLE_FMT_FLT;
        in->sample_rate = kSampleRate;
        av_channel_layout_default(&in->ch_layout, kChannels);
        in->nb_samples = latency;
        in->pts = fed;
        if (av_frame_get_buffer(in, 0) >= 0) {
            std::memset(in->data[0], 0, sizeof(float) * latency * kChannels);
            if (av_buffersrc_add_frame(g.src, in) >= 0)
                drain();
            av_frame_unref(in);
        }
    }
    if (ok) {
        if (av_buffersrc_add_frame(g.src, nullptr) >= 0) // fim: esvazia o filtro
            drain();
    }
    av_frame_free(&in);
    av_frame_free(&filtered);
    out.close();
    if (!ok) {
        QFile::remove(tmp);
        if (error && error->isEmpty())
            *error = QStringLiteral("Falha no filtro de áudio");
        return false;
    }
    QFile::remove(outPath);
    if (!QFile::rename(tmp, outPath))
        return fail(QStringLiteral("Não foi possível gravar %1").arg(outPath));
    return true;
}

void registerDenoised(const QString &mediaPath, double strength, const QString &outPath)
{
    QMutexLocker lock(&g_mutex);
    g_clean.insert(key(mediaPath, strength), outPath);
}

QString registeredDenoised(const QString &mediaPath, double strength)
{
    QMutexLocker lock(&g_mutex);
    return g_clean.value(key(mediaPath, strength));
}

bool discoverDenoised(const QString &mediaPath, double strength)
{
    const QString p = denoisedPathFor(mediaPath, strength);
    if (QFileInfo(p).size() > 44) {
        registerDenoised(mediaPath, strength, p);
        return true;
    }
    return false;
}

} // namespace clv::engine

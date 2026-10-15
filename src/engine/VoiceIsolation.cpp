// SPDX-License-Identifier: GPL-3.0-or-later
#include "VoiceIsolation.h"

#include "Proxy.h"
#include "media/AudioDecoder.h"

#include <QCoreApplication>
#include <QCryptographicHash>
#include <QDateTime>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QHash>
#include <QMutex>

#ifdef CLIVCUT_HAVE_ONNX
#include <onnxruntime_cxx_api.h>
#endif

#include <algorithm>
#include <array>
#include <cmath>
#include <vector>

namespace clv::engine {

using media::kChannels;
using media::kSampleRate;

namespace {

QMutex g_mutex;
QHash<QString, QString> g_voice;

// DeepFilterNet3 (exportação em streaming): quadros de 480 amostras mono a 48 kHz,
// estado de 45304 floats e atraso fixo de 1440 amostras (janela + lookahead).
constexpr int kHop = 480;
constexpr int kStateSize = 45304;
constexpr int kLatency = 1440;

QString key(const QString &path, double strength)
{
    return path + QLatin1Char('|') + QString::number(quantizedVoice(strength), 'f', 1);
}

// Intensidade → limite de atenuação em dB (100 = sem limite: só a voz).
float attenuationLimit(double strength)
{
    const double s = quantizedVoice(strength);
    return s >= 1.0 ? 100.f : static_cast<float>(6 + 30 * s);
}

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

} // namespace

bool voiceIsolationAvailable()
{
#ifdef CLIVCUT_HAVE_ONNX
    return true;
#else
    return false;
#endif
}

double quantizedVoice(double strength)
{
    return std::clamp(std::round(strength * 10) / 10, 0.0, 1.0);
}

QString voicePathFor(const QString &mediaPath, double strength)
{
    const QFileInfo fi(mediaPath);
    const QByteArray k = fi.absoluteFilePath().toUtf8() + '|' + QByteArray::number(fi.size()) + '|'
                         + QByteArray::number(fi.lastModified().toMSecsSinceEpoch()) + "|voice|"
                         + QByteArray::number(quantizedVoice(strength), 'f', 1);
    const QString hash = QString::fromLatin1(QCryptographicHash::hash(k, QCryptographicHash::Sha1).toHex().left(16));
    const QString dir = QDir(QFileInfo(proxyCacheDir()).absolutePath()).filePath(QStringLiteral("audio"));
    return QDir(dir).filePath(QStringLiteral("%1_voice%2_%3.wav")
                                  .arg(fi.completeBaseName().left(40))
                                  .arg(static_cast<int>(quantizedVoice(strength) * 10))
                                  .arg(hash));
}

bool generateVoiceIsolated(const QString &mediaPath, const QString &modelPath, const QString &outPath, double strength,
                           const std::function<bool(double)> &progress, QString *error)
{
    auto fail = [&](const QString &m) {
        if (error)
            *error = m;
        return false;
    };
#ifndef CLIVCUT_HAVE_ONNX
    (void)mediaPath;
    (void)modelPath;
    (void)outPath;
    (void)strength;
    (void)progress;
    return fail(QCoreApplication::translate("Engine", "This build was compiled without ONNX Runtime"));
#else
    if (!QFileInfo::exists(modelPath))
        return fail(QCoreApplication::translate("Engine", "Voice isolation model not found: %1").arg(modelPath));
    media::AudioDecoder dec;
    if (!dec.open(mediaPath, error))
        return false;
    const std::int64_t total = static_cast<std::int64_t>(std::llround(static_cast<double>(dec.duration()) * kSampleRate / kSecond));
    if (total <= 0)
        return fail(QCoreApplication::translate("Engine", "Audio has no duration"));

    QDir().mkpath(QFileInfo(outPath).absolutePath());
    const QString tmp = outPath + QStringLiteral(".part");
    QFile out(tmp);
    if (!out.open(QIODevice::WriteOnly | QIODevice::Truncate))
        return fail(out.errorString());
    out.write(wavHeader(total));

    try {
        Ort::Env env(ORT_LOGGING_LEVEL_WARNING, "clivcut");
        Ort::SessionOptions opts;
        opts.SetIntraOpNumThreads(1); // quadros de 10 ms: paralelizar dentro do modelo não compensa
        opts.SetGraphOptimizationLevel(GraphOptimizationLevel::ORT_ENABLE_ALL);
#ifdef _WIN32
        const std::wstring model = modelPath.toStdWString();
#else
        const std::string model = modelPath.toStdString();
#endif
        Ort::Session session(env, model.c_str(), opts);
        Ort::MemoryInfo mem = Ort::MemoryInfo::CreateCpu(OrtArenaAllocator, OrtMemTypeDefault);
        const char *inNames[] = {"input_frame", "states", "atten_lim_db"};
        const char *outNames[] = {"enhanced_audio_frame", "new_states", "lsnr"};
        const int64_t frameShape[1] = {kHop};
        const int64_t stateShape[1] = {kStateSize};
        std::vector<float> state(kStateSize, 0.f);
        float limit = attenuationLimit(strength);

        // Lê blocos grandes do decodificador e processa quadro a quadro, em mono (a voz fica no centro).
        constexpr int kBlock = kHop * 100; // 1 s
        std::vector<float> stereo(static_cast<size_t>(kBlock) * kChannels), mono(kBlock), enhanced(kBlock);
        std::vector<float> outStereo;
        std::int64_t consumed = 0;    // amostras de entrada já processadas
        std::int64_t written = 0;     // amostras de saída já gravadas (após compensar o atraso)
        const std::int64_t feedTotal = total + kLatency; // cauda de silêncio devolve o final
        while (consumed < feedTotal) {
            const int n = static_cast<int>(std::min<std::int64_t>(kBlock, ((feedTotal - consumed + kHop - 1) / kHop) * kHop));
            std::fill(stereo.begin(), stereo.end(), 0.f);
            if (consumed < total)
                dec.read(static_cast<Micros>(std::llround(static_cast<double>(consumed) * kSecond / kSampleRate)),
                         stereo.data(), static_cast<int>(std::min<std::int64_t>(n, total - consumed)));
            for (int i = 0; i < n; ++i)
                mono[i] = 0.5f * (stereo[i * 2] + stereo[i * 2 + 1]);
            for (int f = 0; f < n; f += kHop) {
                std::array<Ort::Value, 3> in{
                    Ort::Value::CreateTensor<float>(mem, mono.data() + f, kHop, frameShape, 1),
                    Ort::Value::CreateTensor<float>(mem, state.data(), state.size(), stateShape, 1),
                    Ort::Value::CreateTensor<float>(mem, &limit, 1, nullptr, 0)};
                auto res = session.Run(Ort::RunOptions{nullptr}, inNames, in.data(), in.size(), outNames, 3);
                const float *y = res[0].GetTensorData<float>();
                std::copy(y, y + kHop, enhanced.begin() + f);
                const float *s = res[1].GetTensorData<float>();
                std::copy(s, s + kStateSize, state.begin());
            }
            // A saída está atrasada kLatency amostras: descarta o começo e grava só o que cabe no arquivo.
            const std::int64_t skip = std::max<std::int64_t>(0, kLatency - consumed);
            const std::int64_t avail = n - skip;
            const std::int64_t take = std::min<std::int64_t>(avail, total - written);
            if (take > 0) {
                outStereo.resize(static_cast<size_t>(take) * kChannels);
                for (std::int64_t i = 0; i < take; ++i)
                    outStereo[i * 2] = outStereo[i * 2 + 1] = enhanced[skip + i];
                out.write(reinterpret_cast<const char *>(outStereo.data()), take * kChannels * 4);
                written += take;
            }
            consumed += n;
            if (progress && !progress(std::min(1.0, static_cast<double>(consumed) / feedTotal))) {
                out.close();
                QFile::remove(tmp);
                return fail(QCoreApplication::translate("Engine", "Voice isolation cancelled"));
            }
        }
    } catch (const Ort::Exception &e) {
        out.close();
        QFile::remove(tmp);
        return fail(QCoreApplication::translate("Engine", "AI model error: %1").arg(QString::fromUtf8(e.what())));
    }
    out.close();
    QFile::remove(outPath);
    if (!QFile::rename(tmp, outPath))
        return fail(QCoreApplication::translate("Engine", "Could not write %1").arg(outPath));
    return true;
#endif
}

void registerVoice(const QString &mediaPath, double strength, const QString &outPath)
{
    QMutexLocker lock(&g_mutex);
    g_voice.insert(key(mediaPath, strength), outPath);
}

QString registeredVoice(const QString &mediaPath, double strength)
{
    QMutexLocker lock(&g_mutex);
    return g_voice.value(key(mediaPath, strength));
}

bool discoverVoice(const QString &mediaPath, double strength)
{
    const QString p = voicePathFor(mediaPath, strength);
    if (QFileInfo(p).size() > 44) {
        registerVoice(mediaPath, strength, p);
        return true;
    }
    return false;
}

} // namespace clv::engine

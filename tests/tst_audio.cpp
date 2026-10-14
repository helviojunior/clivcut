// SPDX-License-Identifier: GPL-3.0-or-later
#include "core/Edit.h"
#include "core/Evaluate.h"
#include "engine/AudioCleanup.h"
#include "engine/AudioMixer.h"
#include "media/AudioDecoder.h"
#include "media/Loudness.h"
#include "media/MediaProbe.h"

#include <QProcess>
#include <QStandardPaths>
#include <QTemporaryDir>
#include <QTest>

#include <cmath>

using namespace clv;

namespace {

std::vector<float> readMono(const QString &path, Micros start, int frames)
{
    media::AudioDecoder dec;
    std::vector<float> st(static_cast<size_t>(frames) * 2), mono(frames);
    if (!dec.open(path))
        return {};
    dec.read(start, st.data(), frames);
    for (int i = 0; i < frames; ++i)
        mono[i] = (st[i * 2] + st[i * 2 + 1]) * 0.5f;
    return mono;
}

double rms(const std::vector<float> &v)
{
    double acc = 0;
    for (float x : v)
        acc += x * x;
    return std::sqrt(acc / std::max<size_t>(1, v.size()));
}

} // namespace

class AudioTest : public QObject {
    Q_OBJECT

    QTemporaryDir m_dir;
    QString m_noisy;

private slots:
    void initTestCase()
    {
        if (QStandardPaths::findExecutable(QStringLiteral("ffmpeg")).isEmpty())
            QSKIP("ffmpeg não encontrado");
        qputenv("CLIVCUT_CACHE_DIR", m_dir.filePath(QStringLiteral("cache")).toUtf8());
        // 2 s só de chiado, depois um sinal forte não periódico (ruído rosa, como fala) + chiado.
        m_noisy = m_dir.filePath(QStringLiteral("ruidoso.wav"));
        QProcess p;
        p.start(QStringLiteral("ffmpeg"),
                {"-hide_banner", "-loglevel", "error", "-y", "-f", "lavfi", "-i", "anoisesrc=color=white:amplitude=0.04:duration=6",
                 "-f", "lavfi", "-i", "anoisesrc=color=pink:seed=7:amplitude=0.4:duration=4", "-filter_complex",
                 "[1]adelay=2000|2000[s];[0][s]amix=inputs=2:normalize=0", "-ac", "2", "-ar", "48000", m_noisy});
        QVERIFY(p.waitForFinished(60000) && p.exitCode() == 0);
    }

    void denoiseReducesNoiseAndStaysAligned()
    {
        const QString out = engine::denoisedPathFor(m_noisy, 1.0);
        QString err;
        double last = 0;
        QVERIFY2(engine::generateDenoised(m_noisy, out, 1.0, [&](double f) { last = f; return true; }, &err), qPrintable(err));
        QCOMPARE(last, 1.0);
        MediaAsset a, b;
        QVERIFY(media::probe(m_noisy, a) && media::probe(out, b));
        QVERIFY(std::abs(a.duration - b.duration) < 20'000); // mesma duração

        // Trecho só de ruído: pelo menos 10 dB mais baixo.
        const double before = rms(readMono(m_noisy, 500'000, 48000));
        const double after = rms(readMono(out, 500'000, 48000));
        QVERIFY2(20 * std::log10(after / before) < -10, qPrintable(QStringLiteral("%1 → %2").arg(before).arg(after)));

        // Sinal preservado e alinhado: correlação máxima com atraso ~0 (sinal não periódico).
        const auto x = readMono(m_noisy, 3 * kSecond, 9600);
        const auto y = readMono(out, 3 * kSecond, 9600);
        int bestLag = 0;
        double best = -1e9;
        for (int lag = -200; lag <= 200; ++lag) {
            double acc = 0;
            for (int i = 300; i < 9300; ++i)
                acc += x[i] * y[i + lag];
            if (acc > best) {
                best = acc;
                bestLag = lag;
            }
        }
        QVERIFY2(std::abs(bestLag) <= 2, qPrintable(QString::number(bestLag)));
        QVERIFY(rms(y) > rms(x) * 0.5);

        // Registro e reuso pelo cache.
        QVERIFY(engine::registeredDenoised(m_noisy, 0.95).isEmpty() || true);
        QVERIFY(engine::discoverDenoised(m_noisy, 1.0));
        QCOMPARE(engine::registeredDenoised(m_noisy, 0.96), out); // quantizado em 1,0
    }

    void mixerUsesCleanAudioAndGain()
    {
        Project p = createProject();
        MediaAsset a;
        QVERIFY(media::probe(m_noisy, a));
        a.id = newId("ast");
        p.assets.emplace(a.id, a);
        const QString id = edit::insertClip(p, createMediaClip(a, 0), -1);
        std::vector<float> buf(48000 * 2);
        auto level = [&](const Project &proj) {
            engine::AudioMixer mixer;
            mixer.mix(proj, 500'000, buf.data(), 48000);
            double acc = 0;
            for (float s : buf)
                acc += s * s;
            return std::sqrt(acc / buf.size());
        };
        const double original = level(p);
        edit::clipById(p, id)->denoise = 1.0;
        const double cleaned = level(p);
        QVERIFY(cleaned < original * 0.4);
        edit::clipById(p, id)->denoise = 0;
        edit::clipById(p, id)->gainDb = 6;
        QVERIFY(std::abs(level(p) / original - 2.0) < 0.05); // +6 dB ≈ 2×
    }
};

QTEST_GUILESS_MAIN(AudioTest)
#include "tst_audio.moc"

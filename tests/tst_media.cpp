// SPDX-License-Identifier: GPL-3.0-or-later
#include "media/AudioDecoder.h"
#include "media/Loudness.h"
#include "media/MediaProbe.h"
#include "media/VideoDecoder.h"

#include <QImage>
#include <QProcess>
#include <QRegularExpression>
#include <QStandardPaths>
#include <QTemporaryDir>
#include <QTest>

#include <cmath>

using namespace clv;

class MediaTest : public QObject {
    Q_OBJECT

    QTemporaryDir m_dir;
    QString m_video;
    QString m_audio;
    QString m_image;

    bool ffmpeg(const QStringList &args)
    {
        QProcess p;
        p.start(QStringLiteral("ffmpeg"), QStringList{"-hide_banner", "-loglevel", "error", "-y"} + args);
        return p.waitForFinished(60000) && p.exitCode() == 0;
    }

private slots:
    void initTestCase()
    {
        if (QStandardPaths::findExecutable(QStringLiteral("ffmpeg")).isEmpty())
            QSKIP("ffmpeg não encontrado para gerar mídia de teste");
        m_video = m_dir.filePath(QStringLiteral("v.mp4"));
        m_audio = m_dir.filePath(QStringLiteral("a.wav"));
        m_image = m_dir.filePath(QStringLiteral("i.png"));
        QVERIFY(ffmpeg({"-f", "lavfi", "-i", "testsrc2=size=640x360:rate=30:duration=4", "-f", "lavfi", "-i",
                        "sine=frequency=440:duration=4", "-c:v", "libx264", "-g", "30", "-pix_fmt", "yuv420p",
                        "-c:a", "aac", "-shortest", m_video}));
        QVERIFY(ffmpeg({"-f", "lavfi", "-i", "sine=frequency=1000:duration=2", m_audio}));
        QVERIFY(ffmpeg({"-f", "lavfi", "-i", "testsrc2=size=320x240", "-frames:v", "1", m_image}));
    }

    void probeVideo()
    {
        MediaAsset a;
        QString err;
        QVERIFY2(media::probe(m_video, a, &err), qPrintable(err));
        QCOMPARE(a.kind, MediaKind::Video);
        QCOMPARE(a.width, 640);
        QCOMPARE(a.height, 360);
        QVERIFY(a.hasAudio);
        QVERIFY(std::abs(a.duration - 4 * kSecond) < 100'000);
        QCOMPARE(std::lround(a.fps), 30L);
    }

    void probeAudioAndImage()
    {
        MediaAsset a;
        QVERIFY(media::probe(m_audio, a));
        QCOMPARE(a.kind, MediaKind::Audio);
        QVERIFY(!a.hasVideo);
        MediaAsset i;
        QVERIFY(media::probe(m_image, i));
        QCOMPARE(i.kind, MediaKind::Image);
        QCOMPARE(i.width, 320);
    }

    void seekMatchesSequential()
    {
        // Lógica de seek em software (exata); GPU é coberta em hardwareMatchesSoftware.
        media::VideoDecoder seq, rnd;
        QVERIFY(seq.open(m_video, QSize(320, 180), nullptr, false));
        QVERIFY(rnd.open(m_video, QSize(320, 180), nullptr, false));
        QCOMPARE(seq.outputSize(), QSize(320, 180));
        QImage sequential;
        for (int f = 0; f <= 75; ++f)
            sequential = seq.frameAt(f * kSecond / 30 + 1000);
        // Saltar direto para o quadro 75 (2,5 s) deve dar o mesmo quadro.
        const QImage jumped = rnd.frameAt(75 * kSecond / 30 + 1000);
        QVERIFY(!jumped.isNull());
        QCOMPARE(jumped, sequential);
        // Voltar no tempo também funciona.
        const QImage back = rnd.frameAt(1000);
        QVERIFY(back != jumped);
        QCOMPARE(back, media::thumbnail(m_video, 1000, 320));
    }

    void thumbnailAtOneSecond()
    {
        const QImage img = media::thumbnail(m_video, kSecond, 240);
        QVERIFY(!img.isNull());
        QCOMPARE(img.width(), 240);
        // testsrc2 é colorido: a imagem não pode ser uniforme.
        QVERIFY(img.pixelColor(10, img.height() / 2) != img.pixelColor(img.width() - 10, img.height() / 2));
    }

    void hardwareMatchesSoftware()
    {
        media::VideoDecoder hw, sw;
        QVERIFY(hw.open(m_video, QSize(320, 180), nullptr, true));
        QVERIFY(sw.open(m_video, QSize(320, 180), nullptr, false));
        QVERIFY(sw.hardwareDecoder().isEmpty());
        if (hw.hardwareDecoder().isEmpty())
            QSKIP("sem decodificação por hardware nesta máquina");
        qInfo() << "hardware:" << hw.hardwareDecoder();
        media::VideoDecoder hw2;
        QVERIFY(hw2.open(m_video, QSize(320, 180), nullptr, true));
        QImage seq;
        for (int f = 0; f <= 60; ++f)
            seq = hw2.frameAt(f * kSecond / 30 + 1000);
        QCOMPARE(hw.frameAt(60 * kSecond / 30 + 1000), seq); // seek na GPU = leitura sequencial
        for (Micros t : {Micros(0), 2 * kSecond + 1000, kSecond / 2}) { // inclui seek para trás
            const QImage a = hw.frameAt(t).convertToFormat(QImage::Format_RGBA8888);
            const QImage b = sw.frameAt(t).convertToFormat(QImage::Format_RGBA8888);
            QCOMPARE(a.size(), b.size());
            // GPU escala com outro filtro (scale_vaapi): média de erro pequena.
            double diff = 0;
            for (int y = 0; y < a.height(); ++y) {
                const uchar *pa = a.constScanLine(y);
                const uchar *pb = b.constScanLine(y);
                for (int x = 0; x < a.width() * 4; ++x)
                    diff += std::abs(pa[x] - pb[x]);
            }
            diff /= a.width() * a.height() * 4.0;
            QVERIFY2(diff < 8.0, qPrintable(QString::number(diff)));
        }
    }

    void audioRead()
    {
        media::AudioDecoder dec;
        QVERIFY(dec.open(m_audio));
        std::vector<float> buf(4800 * 2);
        QCOMPARE(dec.read(kSecond / 2, buf.data(), 4800), 4800);
        float peak = 0;
        for (float s : buf)
            peak = std::max(peak, std::abs(s));
        QVERIFY2(peak > 0.05f, qPrintable(QString::number(peak))); // seno 0,125 mono → estéreo (-3 dB)
        // Depois do fim: silêncio.
        QCOMPARE(dec.read(3 * kSecond, buf.data(), 4800), 0);
        // Seek para trás.
        QCOMPARE(dec.read(0, buf.data(), 4800), 4800);
    }

    /** Loudness integrada segundo o filtro ebur128 do FFmpeg (referência). */
    double ffmpegLoudness(const QString &file)
    {
        QProcess p;
        p.start(QStringLiteral("ffmpeg"), {"-hide_banner", "-nostats", "-i", file, "-af", "ebur128", "-f", "null", "-"});
        p.waitForFinished(60000);
        const QString out = QString::fromUtf8(p.readAllStandardError());
        const auto m = QRegularExpression(QStringLiteral(R"(I:\s+(-?[\d.]+) LUFS)")).globalMatch(out);
        double last = 0;
        auto it = m;
        while (it.hasNext())
            last = it.next().captured(1).toDouble();
        return last;
    }

    void loudnessMatchesEbur128()
    {
        // Seno, ruído rosa e o vídeo de teste (com áudio): ±0,3 LU do FFmpeg.
        const QString sine = m_dir.filePath(QStringLiteral("sine.wav"));
        const QString pink = m_dir.filePath(QStringLiteral("pink.wav"));
        QVERIFY(ffmpeg({"-f", "lavfi", "-i", "sine=frequency=1000:duration=10", "-af", "volume=-3dB", "-ac", "2", sine}));
        QVERIFY(ffmpeg({"-f", "lavfi", "-i", "anoisesrc=color=pink:duration=8:amplitude=0.3", "-ac", "2", pink}));
        for (const QString &f : {sine, pink, m_video}) {
            double peak = 0;
            const double mine = media::measureLoudness(f, 0, 0, {}, &peak);
            const double ref = ffmpegLoudness(f);
            QVERIFY2(std::abs(mine - ref) < 0.3, qPrintable(QStringLiteral("%1: %2 vs %3").arg(f).arg(mine).arg(ref)));
            QVERIFY(peak > 0);
        }
        // Silêncio: -inf. Trecho: só a parte pedida.
        media::LoudnessMeter silent;
        std::vector<float> zeros(48000 * 2, 0.f);
        silent.add(zeros.data(), 48000);
        QVERIFY(std::isinf(silent.integrated()));
    }

    void waveformPeaks()
    {
        const auto peaks = media::waveform(m_video, 50);
        QCOMPARE(peaks.size(), size_t(50));
        QVERIFY(peaks[25] > 0.05f);
    }
};

QTEST_GUILESS_MAIN(MediaTest)
#include "tst_media.moc"

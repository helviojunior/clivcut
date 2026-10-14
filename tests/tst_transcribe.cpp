// SPDX-License-Identifier: GPL-3.0-or-later
#include "core/Edit.h"
#include "engine/Transcriber.h"
#include "engine/WhisperModels.h"
#include "media/MediaProbe.h"

#include <QFileInfo>
#include <QTest>

using namespace clv;

/**
 * Transcrição real com whisper.cpp sobre o áudio de exemplo do próprio
 * whisper.cpp (discurso de JFK, domínio público). Pulado se o modelo tiny
 * não estiver instalado em ~/.local/share/clivcut/models.
 */
class TranscribeTest : public QObject {
    Q_OBJECT

    QString m_sample = QStringLiteral(CLIVCUT_WHISPER_SAMPLES "/jfk.wav");

private slots:
    void initTestCase()
    {
        if (!engine::transcriptionAvailable())
            QSKIP("build sem whisper.cpp");
        if (!engine::whisperModelInstalled(QStringLiteral("tiny")))
            QSKIP("modelo tiny ausente (baixe com clivcut ou curl)");
        QVERIFY(QFileInfo::exists(m_sample));
    }

    void transcribeTimelineWithOffset()
    {
        Project p = createProject();
        MediaAsset a;
        QVERIFY(media::probe(m_sample, a));
        a.id = newId("ast");
        p.assets.emplace(a.id, a);
        // O áudio começa em 2 s na timeline: as legendas devem acompanhar.
        Clip c = createMediaClip(a, 2 * kSecond);
        edit::insertClip(p, c, -1);

        engine::TranscribeOptions o;
        o.modelPath = engine::whisperModelPath(QStringLiteral("tiny"));
        o.language = QStringLiteral("en");
        o.maxChars = 42;
        double lastProgress = 0;
        std::vector<CaptionSegment> segs;
        QString err;
        QVERIFY2(engine::transcribeProject(p, o, [&](double f) { lastProgress = f; return true; }, segs, &err),
                 qPrintable(err));
        QVERIFY(segs.size() >= 2);
        QString all;
        for (const auto &s : segs) {
            QVERIFY(s.text.size() <= 50);
            all += s.text + QLatin1Char(' ');
        }
        QVERIFY2(all.contains(QStringLiteral("fellow Americans"), Qt::CaseInsensitive), qPrintable(all));
        QVERIFY2(all.contains(QStringLiteral("your country"), Qt::CaseInsensitive), qPrintable(all));
        QVERIFY(segs.front().start >= 2 * kSecond - 100'000);
        QVERIFY(segs.back().end <= 2 * kSecond + a.duration + 500'000);
        QVERIFY(lastProgress > 0.9);
    }

    void silenceIsRejected()
    {
        Project p = createProject();
        engine::TranscribeOptions o;
        o.modelPath = engine::whisperModelPath(QStringLiteral("tiny"));
        std::vector<CaptionSegment> segs;
        QString err;
        QVERIFY(!engine::transcribeProject(p, o, {}, segs, &err));
        QVERIFY(!err.isEmpty());
    }

    void cancel()
    {
        Project p = createProject();
        MediaAsset a;
        QVERIFY(media::probe(m_sample, a));
        a.id = newId("ast");
        p.assets.emplace(a.id, a);
        edit::insertClip(p, createMediaClip(a, 0), -1);
        engine::TranscribeOptions o;
        o.modelPath = engine::whisperModelPath(QStringLiteral("tiny"));
        std::vector<CaptionSegment> segs;
        QString err;
        QVERIFY(!engine::transcribeProject(p, o, [](double f) { return f < 0.05; }, segs, &err));
    }
};

QTEST_GUILESS_MAIN(TranscribeTest)
#include "tst_transcribe.moc"

// SPDX-License-Identifier: GPL-3.0-or-later
#include "core/Edit.h"
#include "core/Evaluate.h"
#include "engine/DecoderFrameSource.h"
#include "engine/Proxy.h"
#include "media/MediaProbe.h"

#include <QFileInfo>
#include <QProcess>
#include <QStandardPaths>
#include <QTemporaryDir>
#include <QTest>

using namespace clv;

class ProxyTest : public QObject {
    Q_OBJECT

    QTemporaryDir m_dir;
    QString m_video;

private slots:
    void initTestCase()
    {
        if (QStandardPaths::findExecutable(QStringLiteral("ffmpeg")).isEmpty())
            QSKIP("ffmpeg não encontrado");
        qputenv("CLIVCUT_CACHE_DIR", m_dir.filePath(QStringLiteral("cache")).toUtf8());
        m_video = m_dir.filePath(QStringLiteral("grande.mp4"));
        QProcess p;
        p.start(QStringLiteral("ffmpeg"), {"-hide_banner", "-loglevel", "error", "-y", "-f", "lavfi", "-i",
                                           "testsrc2=size=2560x1440:rate=30:duration=3", "-c:v", "libx264", "-preset",
                                           "ultrafast", "-pix_fmt", "yuv420p", m_video});
        QVERIFY(p.waitForFinished(120000) && p.exitCode() == 0);
    }

    void recommendation()
    {
        MediaAsset a;
        QVERIFY(media::probe(m_video, a));
        QVERIFY(engine::proxyRecommended(a));
        a.width = 1920;
        a.height = 1080;
        QVERIFY(!engine::proxyRecommended(a));
        a.fps = 120;
        QVERIFY(engine::proxyRecommended(a));
    }

    void generateAndUse()
    {
        const QString out = engine::proxyPathFor(m_video);
        QVERIFY(out.startsWith(m_dir.path()));
        QVERIFY(!engine::discoverProxy(m_video));
        double last = 0;
        QString err;
        QVERIFY2(engine::generateProxy(m_video, out, [&](double f) { last = f; return true; }, &err), qPrintable(err));
        QCOMPARE(last, 1.0);
        MediaAsset p;
        QVERIFY(media::probe(out, p));
        QCOMPARE(p.width, 960);
        QCOMPARE(p.height, 540);
        QVERIFY(std::abs(p.duration - 3 * kSecond) < 100'000);
        QVERIFY(!QFileInfo::exists(out + QStringLiteral(".part.mp4")));

        // Registro + preferência: o preview decodifica o proxy (menor que o original).
        QVERIFY(engine::discoverProxy(m_video));
        QCOMPARE(engine::registeredProxy(m_video), out);
        Project proj = createProject(QStringLiteral("t"), 2560, 1440, 30);
        MediaAsset a;
        QVERIFY(media::probe(m_video, a));
        a.id = newId("ast");
        proj.assets.emplace(a.id, a);
        edit::insertClip(proj, createMediaClip(a, 0), 0);
        const VisualLayer layer = evaluate(proj, kSecond).layers.front();
        engine::DecoderFrameSource original(QSize(2560, 1440), false), proxied(QSize(2560, 1440), true);
        QCOMPARE(original.frameFor(layer).size(), QSize(2560, 1440));
        QCOMPARE(proxied.frameFor(layer).size(), QSize(960, 540));

        // Arquivo original alterado → outro caminho de proxy.
        engine::unregisterProxy(m_video);
        QVERIFY(engine::registeredProxy(m_video).isEmpty());
    }

    void cancel()
    {
        const QString out = m_dir.filePath(QStringLiteral("cancelado.mp4"));
        QString err;
        QVERIFY(!engine::generateProxy(m_video, out, [](double f) { return f < 0.3; }, &err));
        QVERIFY(!QFileInfo::exists(out));
        QVERIFY(!QFileInfo::exists(out + QStringLiteral(".part.mp4")));
    }
};

QTEST_GUILESS_MAIN(ProxyTest)
#include "tst_proxy.moc"

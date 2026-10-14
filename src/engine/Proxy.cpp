// SPDX-License-Identifier: GPL-3.0-or-later
#include "Proxy.h"

#include "media/Encoder.h"
#include "media/MediaProbe.h"
#include "media/VideoDecoder.h"

#include <QCoreApplication>
#include <QCryptographicHash>
#include <QDateTime>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QHash>
#include <QMutex>
#include <QStandardPaths>

#include <algorithm>
#include <cmath>

namespace clv::engine {

namespace {
QMutex g_mutex;
QHash<QString, QString> g_proxies;
} // namespace

QString proxyCacheDir()
{
    QString base = qEnvironmentVariable("CLIVCUT_CACHE_DIR");
    if (base.isEmpty())
        base = QDir(QStandardPaths::writableLocation(QStandardPaths::GenericCacheLocation)).filePath(QStringLiteral("clivcut"));
    return QDir(base).filePath(QStringLiteral("proxies"));
}

QString proxyPathFor(const QString &mediaPath)
{
    const QFileInfo fi(mediaPath);
    const QByteArray key = fi.absoluteFilePath().toUtf8() + '|' + QByteArray::number(fi.size()) + '|' +
                           QByteArray::number(fi.lastModified().toMSecsSinceEpoch());
    const QString hash = QString::fromLatin1(QCryptographicHash::hash(key, QCryptographicHash::Sha1).toHex().left(16));
    return QDir(proxyCacheDir()).filePath(QStringLiteral("%1_%2.mp4").arg(fi.completeBaseName().left(40), hash));
}

bool proxyRecommended(const MediaAsset &asset)
{
    return asset.kind == MediaKind::Video && asset.hasVideo &&
           (std::max(asset.width, asset.height) > 1920 || asset.fps > 60.5);
}

bool generateProxy(const QString &mediaPath, const QString &outPath, const std::function<bool(double)> &progress,
                   QString *error)
{
    auto fail = [&](const QString &m) {
        if (error)
            *error = m;
        return false;
    };
    MediaAsset info;
    if (!media::probe(mediaPath, info, error))
        return false;
    if (info.kind != MediaKind::Video)
        return fail(QCoreApplication::translate("Engine", "Proxies only apply to videos"));

    media::VideoDecoder dec;
    if (!dec.open(mediaPath, QSize(kProxyMaxSide, kProxyMaxSide), error, true))
        return false;
    const QSize size = dec.outputSize();
    const double fps = info.fps > 1 ? std::min(info.fps, 60.0) : 30.0;

    QDir().mkpath(QFileInfo(outPath).absolutePath());
    const QString tmp = outPath + QStringLiteral(".part.mp4");
    QFile::remove(tmp);
    {
        media::EncodeSettings s;
        s.width = size.width();
        s.height = size.height();
        s.fps = fps;
        s.codec = media::VideoCodec::H264;
        s.crf = 23;
        s.preset = QStringLiteral("veryfast");
        s.gopFrames = std::max(1, static_cast<int>(fps / 2)); // seek rápido no scrub
        s.audio = false;
        media::Encoder enc;
        QString err;
        if (!enc.open(tmp, s, &err))
            return fail(err);
        const Micros duration = std::max<Micros>(dec.duration(), info.duration);
        const std::int64_t total = std::max<std::int64_t>(1, std::llround(duration * fps / kSecond));
        for (std::int64_t i = 0; i < total; ++i) {
            const QImage frame = dec.frameAt(static_cast<Micros>(std::llround(i * kSecond / fps)));
            if (frame.isNull())
                break;
            if (frame.size() != size)
                return fail(QCoreApplication::translate("Engine", "Unexpected frame size in the proxy"));
            if (!enc.writeVideo(frame))
                return fail(QCoreApplication::translate("Engine", "Failed to encode the proxy"));
            if (progress && !progress(static_cast<double>(i + 1) / total)) {
                enc.finish();
                QFile::remove(tmp);
                return fail(QCoreApplication::translate("Engine", "Proxy generation cancelled"));
            }
        }
        if (!enc.finish(&err)) {
            QFile::remove(tmp);
            return fail(err);
        }
    }
    QFile::remove(outPath);
    if (!QFile::rename(tmp, outPath))
        return fail(QCoreApplication::translate("Engine", "Could not write %1").arg(outPath));
    return true;
}

void registerProxy(const QString &mediaPath, const QString &proxyPath)
{
    QMutexLocker lock(&g_mutex);
    g_proxies.insert(mediaPath, proxyPath);
}

void unregisterProxy(const QString &mediaPath)
{
    QMutexLocker lock(&g_mutex);
    g_proxies.remove(mediaPath);
}

QString registeredProxy(const QString &mediaPath)
{
    QMutexLocker lock(&g_mutex);
    return g_proxies.value(mediaPath);
}

bool discoverProxy(const QString &mediaPath)
{
    const QString p = proxyPathFor(mediaPath);
    if (QFileInfo(p).size() > 0) {
        registerProxy(mediaPath, p);
        return true;
    }
    return false;
}

} // namespace clv::engine

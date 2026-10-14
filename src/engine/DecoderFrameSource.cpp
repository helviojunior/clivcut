// SPDX-License-Identifier: GPL-3.0-or-later
#include "DecoderFrameSource.h"

#include "Matting.h"
#include "Proxy.h"

#include <QFileInfo>
#include <QSet>

namespace clv::engine {

QImage DecoderFrameSource::frameFor(const VisualLayer &layer)
{
    if (!layer.clip || !layer.asset)
        return {};
    const MediaAsset &a = *layer.asset;
    QString path = a.path;
    if (m_preferProxy) {
        const QString proxy = registeredProxy(a.path);
        if (!proxy.isEmpty())
            path = proxy;
    }
    Entry &e = m_decoders[layer.clip->id];
    if (e.path != path) {
        e = Entry{};
        e.path = path;
    }
    if (e.failed)
        return {};
    if (!e.dec) {
        e.dec = std::make_unique<media::VideoDecoder>();
        if (!e.dec->open(path, m_maxSize)) {
            e.failed = true;
            e.dec.reset();
            return {};
        }
    }
    QImage img = e.dec->frameAt(layer.sourceTime);
    if (layer.clip->removeBackground && !img.isNull())
        applyMatte(e, a.path, layer.sourceTime, img);
    return img;
}

void DecoderFrameSource::applyMatte(Entry &e, const QString &mediaPath, Micros t, QImage &img)
{
    // Sem matte pronto (ainda gerando), o quadro segue opaco.
    const QString path = registeredMatte(mediaPath);
    if (path.isEmpty())
        return;
    if (e.mattePath != path) {
        e.matte.reset();
        e.mattePath = path;
        e.matteFailed = false;
        e.lastMatted = {};
    }
    if (e.matteFailed)
        return;
    // Mesmo quadro de origem (pausado): reaproveita o resultado e a textura.
    const qint64 sourceKey = img.cacheKey();
    if (sourceKey == e.lastSourceKey && !e.lastMatted.isNull()) {
        img = e.lastMatted;
        return;
    }
    if (!e.matte) {
        e.matte = std::make_unique<media::VideoDecoder>();
        if (!e.matte->open(path)) {
            e.matteFailed = true;
            e.matte.reset();
            return;
        }
    }
    QImage m = e.matte->frameAt(t);
    if (m.isNull())
        return;
    if (m.size() != img.size())
        m = m.scaled(img.size(), Qt::IgnoreAspectRatio, Qt::SmoothTransformation);
    m = m.convertToFormat(QImage::Format_RGBA8888);
    if (img.format() != QImage::Format_RGBA8888)
        img = img.convertToFormat(QImage::Format_RGBA8888);
    // O matte é cinza: o canal R vira o alfa (reto) do quadro.
    for (int y = 0; y < img.height(); ++y) {
        uchar *dst = img.scanLine(y);
        const uchar *src = m.constScanLine(y);
        for (int x = 0; x < img.width(); ++x)
            dst[x * 4 + 3] = src[x * 4];
    }
    e.lastSourceKey = sourceKey;
    e.lastMatted = img;
}

void DecoderFrameSource::setMaxSize(QSize s)
{
    if (s == m_maxSize)
        return;
    m_maxSize = s;
    m_decoders.clear();
}

void DecoderFrameSource::prune(const QSet<QString> &liveClipIds)
{
    for (auto it = m_decoders.begin(); it != m_decoders.end();) {
        if (!liveClipIds.contains(it->first))
            it = m_decoders.erase(it);
        else
            ++it;
    }
}

} // namespace clv::engine

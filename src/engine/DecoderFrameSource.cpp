// SPDX-License-Identifier: GPL-3.0-or-later
#include "DecoderFrameSource.h"

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
    return e.dec->frameAt(layer.sourceTime);
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

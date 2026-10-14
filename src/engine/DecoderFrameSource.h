// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once

#include "media/VideoDecoder.h"
#include "render/Compositor.h"

#include <unordered_map>
#include <QSet>
#include <QSize>

#include <memory>

namespace clv::engine {

/**
 * FrameSource que mantém um VideoDecoder por clipe (dois clipes da mesma
 * mídia, p.ex. numa transição, não disputam o mesmo decodificador).
 * `maxSize` limita a resolução decodificada (preview usa algo menor).
 */
class DecoderFrameSource : public render::FrameSource {
public:
    explicit DecoderFrameSource(QSize maxSize = {}, bool preferProxy = false)
        : m_maxSize(maxSize), m_preferProxy(preferProxy) {}

    QImage frameFor(const VisualLayer &layer) override;

    void setMaxSize(QSize s);
    /// Usa proxies registrados (engine/Proxy.h) quando houver.
    void setPreferProxy(bool on) { m_preferProxy = on; }
    /** Fecha decodificadores de clipes que não estão mais no projeto. */
    void prune(const QSet<QString> &liveClipIds);
    void clear() { m_decoders.clear(); }

private:
    struct Entry {
        std::unique_ptr<media::VideoDecoder> dec;
        QString path;
        bool failed = false;
    };
    std::unordered_map<QString, Entry> m_decoders;
    QSize m_maxSize;
    bool m_preferProxy;
};

} // namespace clv::engine

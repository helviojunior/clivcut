// SPDX-License-Identifier: GPL-3.0-or-later
#include "PreviewItem.h"

#include "ProjectController.h"
#include "core/Evaluate.h"
#include "engine/DecoderFrameSource.h"
#include "render/Compositor.h"

#include <QSet>

#include <algorithm>
#include <cmath>
#include <memory>

namespace {

QRectF fitRect(QSizeF content, QSizeF box)
{
    if (content.isEmpty() || box.isEmpty())
        return {};
    const QSizeF s = content.scaled(box, Qt::KeepAspectRatio);
    return {(box.width() - s.width()) / 2, (box.height() - s.height()) / 2, s.width(), s.height()};
}

class PreviewRenderer : public QQuickRhiItemRenderer {
public:
    void initialize(QRhiCommandBuffer *) override
    {
        if (!m_compositor || m_rhi != rhi()) {
            m_rhi = rhi();
            m_compositor = std::make_unique<clv::render::Compositor>(m_rhi);
        }
    }

    void synchronize(QQuickRhiItem *item) override
    {
        auto *ctrl = ProjectController::instance();
        if (!ctrl)
            return;
        m_project = ctrl->snapshot();
        m_time = static_cast<clv::Micros>(ctrl->playhead());
        m_maxRes = static_cast<PreviewItem *>(item)->maxResolution();
        m_playing = ctrl->playing();
        m_frames.setPreferProxy(ctrl->useProxies());
    }

    void render(QRhiCommandBuffer *cb) override
    {
        QRhiRenderTarget *rt = renderTarget();
        const QSize out = rt->pixelSize();
        if (!m_project) {
            return;
        }
        const QRectF vp = fitRect(QSizeF(m_project->width, m_project->height), QSizeF(out));
        // Resolução de composição: a da área visível, limitada por maxResolution.
        QSize renderSize = vp.size().toSize();
        const int longSide = std::max(renderSize.width(), renderSize.height());
        if (longSide > m_maxRes)
            renderSize = (QSizeF(renderSize) * (static_cast<double>(m_maxRes) / longSide)).toSize();
        renderSize = renderSize.expandedTo(QSize(16, 16));
        m_compositor->setRenderSize(renderSize);
        m_frames.setMaxSize(renderSize);

        const clv::FrameDesc frame = clv::evaluate(*m_project, m_time);
        if (++m_pruneCounter % 120 == 0) {
            QSet<QString> live;
            for (const auto &t : m_project->tracks)
                for (const auto &c : t.clips)
                    live.insert(c.id);
            m_frames.prune(live);
        }
        m_compositor->composite(cb, *m_project, frame, m_frames);
        m_compositor->blitTo(cb, rt, vp, QColor(0x14, 0x14, 0x17));
    }

private:
    QRhi *m_rhi = nullptr;
    std::unique_ptr<clv::render::Compositor> m_compositor;
    clv::engine::DecoderFrameSource m_frames;
    std::shared_ptr<const clv::Project> m_project;
    clv::Micros m_time = 0;
    int m_maxRes = 1280;
    bool m_playing = false;
    int m_pruneCounter = 0;
};

} // namespace

PreviewItem::PreviewItem(QQuickItem *parent) : QQuickRhiItem(parent)
{
    if (auto *ctrl = ProjectController::instance()) {
        connect(ctrl, &ProjectController::revisionChanged, this, &QQuickItem::update);
        connect(ctrl, &ProjectController::playheadChanged, this, &QQuickItem::update);
        connect(ctrl, &ProjectController::projectChanged, this, &PreviewItem::canvasRectChanged);
    }
}

QQuickRhiItemRenderer *PreviewItem::createRenderer()
{
    return new PreviewRenderer;
}

QRectF PreviewItem::canvasRect() const
{
    auto *ctrl = ProjectController::instance();
    if (!ctrl)
        return {};
    return fitRect(QSizeF(ctrl->canvasWidth(), ctrl->canvasHeight()), size());
}

void PreviewItem::setMaxResolution(int r)
{
    if (r == m_maxResolution)
        return;
    m_maxResolution = r;
    emit maxResolutionChanged();
    update();
}

void PreviewItem::geometryChange(const QRectF &newGeometry, const QRectF &oldGeometry)
{
    QQuickRhiItem::geometryChange(newGeometry, oldGeometry);
    emit canvasRectChanged();
}

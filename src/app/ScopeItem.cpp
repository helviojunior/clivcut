// SPDX-License-Identifier: GPL-3.0-or-later
#include "ScopeItem.h"

#include "ProjectController.h"
#include "Scopes.h"

#include <QPainter>
#include <QQuickWindow>

ScopeItem::ScopeItem(QQuickItem *parent) : QQuickPaintedItem(parent)
{
    if (auto *ctrl = ProjectController::instance())
        connect(ctrl, &ProjectController::scopeFrameChanged, this, &ScopeItem::refresh);
    connect(this, &QQuickItem::widthChanged, this, &ScopeItem::refresh);
    connect(this, &QQuickItem::heightChanged, this, &ScopeItem::refresh);
}

void ScopeItem::setMode(int m)
{
    if (m == m_mode)
        return;
    m_mode = m;
    emit modeChanged();
    refresh();
}

void ScopeItem::refresh()
{
    auto *ctrl = ProjectController::instance();
    if (!ctrl || !isVisible() || !window())
        return;
    const QSize size = (QSizeF(width(), height()) * window()->effectiveDevicePixelRatio()).toSize();
    m_plot = clv::app::renderScope(ctrl->scopeFrame(), static_cast<clv::app::ScopeMode>(m_mode), size);
    update();
}

void ScopeItem::paint(QPainter *painter)
{
    if (!m_plot.isNull())
        painter->drawImage(QRectF(0, 0, width(), height()), m_plot);
}

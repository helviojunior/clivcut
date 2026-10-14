// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once

#include <QQuickRhiItem>
#include <QtQml/qqmlregistration.h>

/**
 * Viewer do projeto: compõe o quadro no cursor com o mesmo compositor do
 * export, direto na GPU do Qt Quick (sem cópias para a CPU).
 */
class PreviewItem : public QQuickRhiItem {
    Q_OBJECT
    QML_ELEMENT
    /// Retângulo (em coordenadas do item) onde o canvas aparece, para overlays.
    Q_PROPERTY(QRectF canvasRect READ canvasRect NOTIFY canvasRectChanged)
    /// Resolução máxima do preview (lado maior), para equilibrar fluidez/qualidade.
    Q_PROPERTY(int maxResolution READ maxResolution WRITE setMaxResolution NOTIFY maxResolutionChanged)

public:
    explicit PreviewItem(QQuickItem *parent = nullptr);

    QRectF canvasRect() const;
    int maxResolution() const { return m_maxResolution; }
    void setMaxResolution(int r);

signals:
    void canvasRectChanged();
    void maxResolutionChanged();

protected:
    QQuickRhiItemRenderer *createRenderer() override;
    void geometryChange(const QRectF &newGeometry, const QRectF &oldGeometry) override;

private:
    int m_maxResolution = 1280;
};

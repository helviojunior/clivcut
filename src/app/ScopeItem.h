// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once

#include <QImage>
#include <QQuickPaintedItem>
#include <QtQml/qqmlregistration.h>

/** Desenha um escopo (histograma, forma de onda, vetorscópio) do quadro atual do player. */
class ScopeItem : public QQuickPaintedItem {
    Q_OBJECT
    QML_ELEMENT
    /// 0 = histograma, 1 = forma de onda, 2 = vetorscópio
    Q_PROPERTY(int mode READ mode WRITE setMode NOTIFY modeChanged)

public:
    explicit ScopeItem(QQuickItem *parent = nullptr);
    int mode() const { return m_mode; }
    void setMode(int m);
    void paint(QPainter *painter) override;

signals:
    void modeChanged();

private:
    void refresh();
    int m_mode = 0;
    QImage m_plot;
};

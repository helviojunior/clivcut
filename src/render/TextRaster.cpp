// SPDX-License-Identifier: GPL-3.0-or-later
#include "TextRaster.h"

#include <QFont>
#include <QFontMetricsF>
#include <QHash>
#include <QPainter>
#include <QPainterPath>

#include <algorithm>
#include <cmath>

namespace clv::render {

QImage rasterizeText(const Clip &clip, double pxScale)
{
    const TextStyle &s = clip.style;
    QFont font(s.fontFamily);
    font.setPixelSize(std::max(1, static_cast<int>(std::lround(s.fontSize * pxScale))));
    font.setBold(s.bold);
    font.setItalic(s.italic);
    font.setStyleStrategy(QFont::PreferAntialias);
    const QFontMetricsF fm(font);

    const QStringList lines = clip.text.isEmpty() ? QStringList{QStringLiteral(" ")} : clip.text.split(QLatin1Char('\n'));
    double textW = 1;
    for (const auto &l : lines)
        textW = std::max(textW, fm.horizontalAdvance(l));
    const double lineH = fm.lineSpacing();
    const double textH = lineH * lines.size();

    const double stroke = s.strokeWidth * pxScale;
    const double shadowOff = s.shadow ? std::max(1.0, font.pixelSize() * 0.05) : 0.0;
    const double bgPad = s.background.isEmpty() ? 0.0 : font.pixelSize() * 0.3;
    const double pad = std::ceil(stroke + shadowOff + bgPad + 2);

    const QSize size(static_cast<int>(std::ceil(textW + 2 * pad)), static_cast<int>(std::ceil(textH + 2 * pad)));
    QImage img(size, QImage::Format_ARGB32_Premultiplied);
    img.fill(Qt::transparent);
    QPainter p(&img);
    p.setRenderHints(QPainter::Antialiasing | QPainter::TextAntialiasing);

    if (!s.background.isEmpty()) {
        p.setPen(Qt::NoPen);
        p.setBrush(QColor(s.background));
        const double r = font.pixelSize() * 0.2;
        p.drawRoundedRect(QRectF(pad - bgPad, pad - bgPad, textW + 2 * bgPad, textH + 2 * bgPad), r, r);
    }

    QPainterPath path;
    for (int i = 0; i < lines.size(); ++i) {
        const double w = fm.horizontalAdvance(lines[i]);
        double x = pad;
        if (s.align == 1)
            x += (textW - w) / 2;
        else if (s.align == 2)
            x += textW - w;
        path.addText(QPointF(x, pad + i * lineH + fm.ascent()), font, lines[i]);
    }

    if (s.shadow) {
        p.setPen(Qt::NoPen);
        p.setBrush(QColor(0, 0, 0, 140));
        p.drawPath(path.translated(shadowOff, shadowOff));
    }
    if (stroke > 0.01) {
        QPen pen(QColor(s.strokeColor), stroke * 2, Qt::SolidLine, Qt::RoundCap, Qt::RoundJoin);
        p.strokePath(path, pen);
    }
    p.fillPath(path, QColor(s.color));
    p.end();
    return img.convertToFormat(QImage::Format_RGBA8888);
}

quint64 textCacheKey(const Clip &clip, double pxScale)
{
    const TextStyle &s = clip.style;
    size_t h = qHashMulti(0, clip.text, s.fontFamily, s.fontSize, s.bold, s.italic, s.color, s.strokeColor);
    h = qHashMulti(h, s.strokeWidth, s.background, s.align, s.shadow, std::lround(pxScale * 10000));
    return h;
}

} // namespace clv::render

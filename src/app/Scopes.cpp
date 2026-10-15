// SPDX-License-Identifier: GPL-3.0-or-later
#include "Scopes.h"

#include <QPainter>
#include <QPainterPath>

#include <algorithm>
#include <cmath>
#include <vector>

namespace clv::app {

namespace {

constexpr double kLumaR = 0.2126, kLumaG = 0.7152, kLumaB = 0.0722;

int lumaOf(const uchar *p) { return static_cast<int>(kLumaR * p[0] + kLumaG * p[1] + kLumaB * p[2] + 0.5); }

// Intensidade logarítmica: poucos pixels já aparecem, muitos não estouram.
int glow(int count, int peak) { return peak <= 0 ? 0 : static_cast<int>(255 * std::log1p(count) / std::log1p(peak)); }

QImage drawHistogram(const QImage &frame, QSize size)
{
    QImage out(size, QImage::Format_ARGB32_Premultiplied);
    out.fill(Qt::transparent);
    const auto h = histogram(frame);
    int peak = 1;
    for (int ch = 0; ch < 3; ++ch)
        for (int i = 1; i < 255; ++i) // pretos e brancos puros não definem a escala
            peak = std::max(peak, h[ch][i]);
    QPainter p(&out);
    p.setRenderHint(QPainter::Antialiasing);
    p.setCompositionMode(QPainter::CompositionMode_Plus);
    const QColor colors[] = {QColor(255, 70, 70, 150), QColor(70, 220, 110, 150), QColor(80, 140, 255, 150)};
    for (int ch = 0; ch < 3; ++ch) {
        QPainterPath path;
        path.moveTo(0, size.height());
        for (int i = 0; i < 256; ++i) {
            const double x = i * (size.width() - 1) / 255.0;
            const double y = size.height() - std::min(1.0, static_cast<double>(h[ch][i]) / peak) * (size.height() - 2);
            path.lineTo(x, y);
        }
        path.lineTo(size.width(), size.height());
        path.closeSubpath();
        p.fillPath(path, colors[ch]);
    }
    return out;
}

QImage drawWaveform(const QImage &frame, QSize size)
{
    // Grade coluna × nível de luminância; cada pixel do quadro soma 1 na sua coluna.
    const int W = size.width(), H = size.height();
    std::vector<int> grid(static_cast<size_t>(W) * H, 0);
    for (int y = 0; y < frame.height(); ++y) {
        const uchar *row = frame.constScanLine(y);
        for (int x = 0; x < frame.width(); ++x) {
            const int col = x * W / frame.width();
            const int lv = H - 1 - lumaOf(row + x * 4) * (H - 1) / 255;
            ++grid[static_cast<size_t>(lv) * W + col];
        }
    }
    const int peak = *std::max_element(grid.begin(), grid.end());
    QImage out(size, QImage::Format_ARGB32_Premultiplied);
    for (int y = 0; y < H; ++y) {
        auto *line = reinterpret_cast<QRgb *>(out.scanLine(y));
        for (int x = 0; x < W; ++x) {
            const int a = glow(grid[static_cast<size_t>(y) * W + x], peak);
            line[x] = qPremultiply(qRgba(120, 255, 150, a));
        }
    }
    QPainter p(&out);
    p.setPen(QColor(255, 255, 255, 40));
    for (int i = 1; i < 4; ++i)
        p.drawLine(0, H * i / 4, W, H * i / 4);
    return out;
}

QImage drawVectorscope(const QImage &frame, QSize size)
{
    const int S = std::min(size.width(), size.height());
    std::vector<int> grid(static_cast<size_t>(S) * S, 0);
    std::vector<QRgb> tint(static_cast<size_t>(S) * S, 0);
    const double r = S / 2.0 - 2;
    for (int y = 0; y < frame.height(); ++y) {
        const uchar *row = frame.constScanLine(y);
        for (int x = 0; x < frame.width(); ++x) {
            const uchar *px = row + x * 4;
            const double R = px[0] / 255.0, G = px[1] / 255.0, B = px[2] / 255.0;
            const double Y = kLumaR * R + kLumaG * G + kLumaB * B;
            const double u = (B - Y) / 1.8556, v = (R - Y) / 1.5748; // Cb, Cr em -0,5..0,5
            const int gx = std::clamp(static_cast<int>(S / 2.0 + u * 2 * r), 0, S - 1);
            const int gy = std::clamp(static_cast<int>(S / 2.0 - v * 2 * r), 0, S - 1);
            const size_t i = static_cast<size_t>(gy) * S + gx;
            ++grid[i];
            tint[i] = qRgb(px[0], px[1], px[2]);
        }
    }
    const int peak = *std::max_element(grid.begin(), grid.end());
    QImage out(size, QImage::Format_ARGB32_Premultiplied);
    out.fill(Qt::transparent);
    const int ox = (size.width() - S) / 2, oy = (size.height() - S) / 2;
    for (int y = 0; y < S; ++y) {
        auto *line = reinterpret_cast<QRgb *>(out.scanLine(y + oy));
        for (int x = 0; x < S; ++x) {
            const size_t i = static_cast<size_t>(y) * S + x;
            if (!grid[i])
                continue;
            // Cor do próprio pixel, clareada: dá para ver de onde vem cada mancha.
            const QColor c = QColor(tint[i]).lighter(150);
            line[x + ox] = qPremultiply(qRgba(c.red(), c.green(), c.blue(), std::max(60, glow(grid[i], peak))));
        }
    }
    // Retícula: círculo, alvos das cores primárias/secundárias a 75% e a linha de tom de pele.
    QPainter p(&out);
    p.setRenderHint(QPainter::Antialiasing);
    const QPointF c(size.width() / 2.0, size.height() / 2.0);
    p.setPen(QColor(255, 255, 255, 50));
    p.drawEllipse(c, r, r);
    p.drawLine(QPointF(c.x() - r, c.y()), QPointF(c.x() + r, c.y()));
    p.drawLine(QPointF(c.x(), c.y() - r), QPointF(c.x(), c.y() + r));
    const struct { double R, G, B; const char *name; } targets[] = {
        {0.75, 0, 0, "R"}, {0.75, 0.75, 0, "Yl"}, {0, 0.75, 0, "G"}, {0, 0.75, 0.75, "Cy"}, {0, 0, 0.75, "B"}, {0.75, 0, 0.75, "Mg"}};
    QFont f = p.font();
    f.setPixelSize(9);
    p.setFont(f);
    for (const auto &t : targets) {
        const double Y = kLumaR * t.R + kLumaG * t.G + kLumaB * t.B;
        const QPointF pt(c.x() + (t.B - Y) / 1.8556 * 2 * r, c.y() - (t.R - Y) / 1.5748 * 2 * r);
        p.setPen(QColor(255, 255, 255, 110));
        p.drawRect(QRectF(pt.x() - 3, pt.y() - 3, 6, 6));
        p.drawText(pt + QPointF(5, -4), QString::fromLatin1(t.name));
    }
    // Tom de pele: matiz em torno de 123° no plano Cb/Cr (entre vermelho e amarelo).
    p.setPen(QPen(QColor(255, 190, 140, 110), 1, Qt::DashLine));
    const double a = qDegreesToRadians(123.0);
    p.drawLine(c, QPointF(c.x() + std::cos(a) * r, c.y() - std::sin(a) * r));
    return out;
}

} // namespace

std::array<std::array<int, 256>, 4> histogram(const QImage &src)
{
    std::array<std::array<int, 256>, 4> h{};
    const QImage frame = src.format() == QImage::Format_RGBA8888 ? src : src.convertToFormat(QImage::Format_RGBA8888);
    for (int y = 0; y < frame.height(); ++y) {
        const uchar *row = frame.constScanLine(y);
        for (int x = 0; x < frame.width(); ++x) {
            const uchar *p = row + x * 4;
            ++h[0][p[0]];
            ++h[1][p[1]];
            ++h[2][p[2]];
            ++h[3][lumaOf(p)];
        }
    }
    return h;
}

QImage renderScope(const QImage &src, ScopeMode mode, QSize size)
{
    if (src.isNull() || size.isEmpty())
        return {};
    const QImage frame = src.format() == QImage::Format_RGBA8888 ? src : src.convertToFormat(QImage::Format_RGBA8888);
    switch (mode) {
    case ScopeMode::Histogram: return drawHistogram(frame, size);
    case ScopeMode::Waveform: return drawWaveform(frame, size);
    case ScopeMode::Vectorscope: return drawVectorscope(frame, size);
    }
    return {};
}

} // namespace clv::app

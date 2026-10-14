// SPDX-License-Identifier: GPL-3.0-or-later
#include "SpeedCurve.h"

#include <QCoreApplication>

#include <algorithm>
#include <cmath>

namespace clv {

namespace {

/** Integral da curva de 0 até u (trapézios exatos para a linear por partes). */
double integralTo(const std::vector<SpeedPoint> &c, double u)
{
    double acc = 0;
    for (size_t i = 0; i + 1 < c.size(); ++i) {
        const SpeedPoint &a = c[i], &b = c[i + 1];
        if (u <= a.u)
            break;
        const double end = std::min(u, b.u);
        const double span = b.u - a.u;
        const double vEnd = span > 0 ? a.v + (b.v - a.v) * (end - a.u) / span : b.v;
        acc += (a.v + vEnd) * 0.5 * (end - a.u);
    }
    return acc;
}

} // namespace

double curveValue(const std::vector<SpeedPoint> &c, double u)
{
    if (c.empty())
        return 1;
    if (u <= c.front().u)
        return c.front().v;
    for (size_t i = 0; i + 1 < c.size(); ++i)
        if (u <= c[i + 1].u) {
            const double span = c[i + 1].u - c[i].u;
            return span > 0 ? c[i].v + (c[i + 1].v - c[i].v) * (u - c[i].u) / span : c[i + 1].v;
        }
    return c.back().v;
}

double curveMean(const std::vector<SpeedPoint> &c)
{
    if (c.size() < 2)
        return c.empty() ? 1 : c.front().v;
    return integralTo(c, 1.0);
}

double curveProgress(const std::vector<SpeedPoint> &c, double u)
{
    u = std::clamp(u, 0.0, 1.0);
    if (c.size() < 2)
        return u;
    const double total = integralTo(c, 1.0);
    return total > 0 ? integralTo(c, u) / total : u;
}

double speedAt(const Clip &c, Micros rel)
{
    if (c.speedCurve.size() < 2 || c.duration <= 0)
        return c.speed;
    const double mean = curveMean(c.speedCurve);
    const double u = std::clamp(static_cast<double>(rel) / c.duration, 0.0, 1.0);
    // speed guarda a média; a curva dá a forma relativa.
    return mean > 0 ? c.speed * curveValue(c.speedCurve, u) / mean : c.speed;
}

std::vector<SpeedPoint> normalizedCurve(std::vector<SpeedPoint> c)
{
    for (auto &p : c) {
        p.u = std::clamp(p.u, 0.0, 1.0);
        p.v = std::clamp(p.v, 0.1, 10.0);
    }
    std::sort(c.begin(), c.end(), [](const auto &a, const auto &b) { return a.u < b.u; });
    if (c.empty())
        return c;
    if (c.front().u > 0)
        c.insert(c.begin(), {0, c.front().v});
    if (c.back().u < 1)
        c.push_back({1, c.back().v});
    return c;
}

std::vector<SpeedPoint> sliceCurve(const std::vector<SpeedPoint> &c, double a, double b)
{
    if (c.size() < 2 || b <= a)
        return c;
    std::vector<SpeedPoint> out{{0, curveValue(c, a)}};
    for (const auto &p : c)
        if (p.u > a && p.u < b)
            out.push_back({(p.u - a) / (b - a), p.v});
    out.push_back({1, curveValue(c, b)});
    return out;
}

const std::vector<SpeedPreset> &speedPresets()
{
    static const std::vector<SpeedPreset> presets{
        {QStringLiteral("montage"), QString::fromUtf8(QT_TRANSLATE_NOOP("SpeedCurves", "Montage")),
         {{0, 3}, {0.3, 3}, {0.4, 0.5}, {0.6, 0.5}, {0.7, 3}, {1, 3}}},
        {QStringLiteral("hero"), QString::fromUtf8(QT_TRANSLATE_NOOP("SpeedCurves", "Hero")), {{0, 1.5}, {0.35, 1.5}, {0.5, 0.3}, {0.65, 1.5}, {1, 1.5}}},
        {QStringLiteral("bullet"), QString::fromUtf8(QT_TRANSLATE_NOOP("SpeedCurves", "Bullet")), {{0, 4}, {0.4, 0.2}, {0.6, 0.2}, {1, 4}}},
        {QStringLiteral("jump"), QString::fromUtf8(QT_TRANSLATE_NOOP("SpeedCurves", "Jump")), {{0, 0.6}, {0.45, 0.6}, {0.5, 5}, {0.55, 0.6}, {1, 0.6}}},
        {QStringLiteral("flashIn"), QString::fromUtf8(QT_TRANSLATE_NOOP("SpeedCurves", "Flash in")), {{0, 6}, {0.3, 1}, {1, 1}}},
        {QStringLiteral("flashOut"), QString::fromUtf8(QT_TRANSLATE_NOOP("SpeedCurves", "Flash out")), {{0, 1}, {0.7, 1}, {1, 6}}},
    };
    return presets;
}

} // namespace clv

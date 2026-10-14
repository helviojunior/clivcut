// SPDX-License-Identifier: GPL-3.0-or-later
#include "Keyframes.h"

#include <algorithm>
#include <cmath>
#include <cstdlib>

namespace clv {

double applyEasing(Easing e, double u)
{
    switch (e) {
    case Easing::Linear: return u;
    case Easing::EaseIn: return u * u * u;
    case Easing::EaseOut: return 1 - std::pow(1 - u, 3);
    case Easing::EaseInOut: return u < 0.5 ? 4 * u * u * u : 1 - std::pow(-2 * u + 2, 3) / 2;
    case Easing::Hold: return 0;
    case Easing::Bezier: return cubicBezier(0.33, 0, 0.67, 1, u); // sem pontos: padrão suave
    }
    return u;
}

double cubicBezier(double x1, double y1, double x2, double y2, double u)
{
    x1 = std::clamp(x1, 0.0, 1.0);
    x2 = std::clamp(x2, 0.0, 1.0);
    auto bez = [](double a, double b, double s) {
        const double m = 1 - s;
        return 3 * m * m * s * a + 3 * m * s * s * b + s * s * s;
    };
    auto dbez = [](double a, double b, double s) {
        const double m = 1 - s;
        return 3 * m * m * a + 6 * m * s * (b - a) + 3 * s * s * (1 - b);
    };
    // Acha s tal que x(s) = u: Newton, com bisseção como rede de segurança.
    double s = u;
    for (int i = 0; i < 8; ++i) {
        const double x = bez(x1, x2, s) - u;
        const double d = dbez(x1, x2, s);
        if (std::abs(x) < 1e-7)
            return bez(y1, y2, s);
        if (std::abs(d) < 1e-6)
            break;
        s = std::clamp(s - x / d, 0.0, 1.0);
    }
    double lo = 0, hi = 1;
    s = u;
    for (int i = 0; i < 40; ++i) {
        const double x = bez(x1, x2, s);
        if (std::abs(x - u) < 1e-7)
            break;
        (x < u ? lo : hi) = s;
        s = (lo + hi) / 2;
    }
    return bez(y1, y2, s);
}

double segmentEase(const Keyframe &a, double u)
{
    if (a.ease == Easing::Bezier)
        return cubicBezier(a.bx1, a.by1, a.bx2, a.by2, u);
    return applyEasing(a.ease, u);
}

double valueAt(const std::vector<Keyframe> &kfs, Micros t, double base)
{
    if (kfs.empty())
        return base;
    if (t <= kfs.front().t)
        return kfs.front().v;
    if (t >= kfs.back().t)
        return kfs.back().v;
    auto it = std::upper_bound(kfs.begin(), kfs.end(), t, [](Micros x, const Keyframe &k) { return x < k.t; });
    const Keyframe &b = *it;
    const Keyframe &a = *(it - 1);
    const double u = static_cast<double>(t - a.t) / static_cast<double>(b.t - a.t);
    return a.v + (b.v - a.v) * segmentEase(a, u);
}

double baseValue(const Clip &c, Prop p)
{
    switch (p) {
    case Prop::X: return c.transform.x;
    case Prop::Y: return c.transform.y;
    case Prop::Scale: return c.transform.scale;
    case Prop::Rotation: return c.transform.rotation;
    case Prop::Opacity: return c.transform.opacity;
    case Prop::Volume: return c.volume;
    case Prop::Brightness: return c.color.brightness;
    case Prop::Contrast: return c.color.contrast;
    case Prop::Saturation: return c.color.saturation;
    case Prop::Count: break;
    }
    return 0;
}

void setBaseValue(Clip &c, Prop p, double v)
{
    switch (p) {
    case Prop::X: c.transform.x = v; break;
    case Prop::Y: c.transform.y = v; break;
    case Prop::Scale: c.transform.scale = v; break;
    case Prop::Rotation: c.transform.rotation = v; break;
    case Prop::Opacity: c.transform.opacity = v; break;
    case Prop::Volume: c.volume = v; break;
    case Prop::Brightness: c.color.brightness = v; break;
    case Prop::Contrast: c.color.contrast = v; break;
    case Prop::Saturation: c.color.saturation = v; break;
    case Prop::Count: break;
    }
}

double propAt(const Clip &c, Prop p, Micros rel)
{
    return valueAt(c.keyframes[static_cast<int>(p)], rel, baseValue(c, p));
}

void upsertKeyframe(std::vector<Keyframe> &kfs, Keyframe kf, Micros tolerance)
{
    std::erase_if(kfs, [&](const Keyframe &k) { return std::llabs(k.t - kf.t) <= tolerance; });
    kfs.push_back(kf);
    std::sort(kfs.begin(), kfs.end(), [](const Keyframe &a, const Keyframe &b) { return a.t < b.t; });
}

bool removeKeyframeAt(std::vector<Keyframe> &kfs, Micros t, Micros tolerance)
{
    return std::erase_if(kfs, [&](const Keyframe &k) { return std::llabs(k.t - t) <= tolerance; }) > 0;
}

const Keyframe *keyframeAt(const std::vector<Keyframe> &kfs, Micros t, Micros tolerance)
{
    for (const auto &k : kfs)
        if (std::llabs(k.t - t) <= tolerance)
            return &k;
    return nullptr;
}

std::pair<std::vector<Keyframe>, std::vector<Keyframe>> splitKeyframes(const std::vector<Keyframe> &kfs, Micros at)
{
    if (kfs.empty())
        return {};
    const double v = valueAt(kfs, at, kfs.front().v);
    Keyframe current;
    for (const auto &k : kfs)
        if (k.t <= at)
            current = k;
    const Easing easeAt = current.ease;
    std::vector<Keyframe> left, right;
    for (const auto &k : kfs) {
        if (k.t < at)
            left.push_back(k);
        else if (k.t > at)
            right.push_back({k.t - at, k.v, k.ease});
    }
    // O limite sempre recebe o valor interpolado para não "pular" a animação.
    Keyframe boundary = current;
    boundary.v = v;
    boundary.ease = easeAt;
    boundary.t = at;
    left.push_back(boundary);
    boundary.t = 0;
    right.insert(right.begin(), boundary);
    return {left, right};
}

void clampKeyframes(std::vector<Keyframe> &kfs, Micros duration)
{
    if (kfs.empty())
        return;
    const double v0 = valueAt(kfs, 0, 0);
    const double v1 = valueAt(kfs, duration, 0);
    const bool hadBefore = kfs.front().t < 0;
    const bool hadAfter = kfs.back().t > duration;
    std::erase_if(kfs, [&](const Keyframe &k) { return k.t < 0 || k.t > duration; });
    if (hadBefore)
        upsertKeyframe(kfs, {0, v0, Easing::Linear}, 0);
    if (hadAfter)
        upsertKeyframe(kfs, {duration, v1, Easing::Linear}, 0);
}

void shiftKeyframes(std::vector<Keyframe> &kfs, Micros delta)
{
    for (auto &k : kfs)
        k.t += delta;
}

const char *propName(Prop p)
{
    switch (p) {
    case Prop::X: return "x";
    case Prop::Y: return "y";
    case Prop::Scale: return "scale";
    case Prop::Rotation: return "rotation";
    case Prop::Opacity: return "opacity";
    case Prop::Volume: return "volume";
    case Prop::Brightness: return "brightness";
    case Prop::Contrast: return "contrast";
    case Prop::Saturation: return "saturation";
    case Prop::Count: break;
    }
    return "";
}

} // namespace clv

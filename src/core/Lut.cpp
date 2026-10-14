// SPDX-License-Identifier: GPL-3.0-or-later
#include "Lut.h"

#include <QCoreApplication>
#include <QFile>
#include <QHash>
#include <QMutex>
#include <QTextStream>

#include <algorithm>
#include <cmath>
#include <functional>

namespace clv {

namespace {

using Vec3 = std::array<float, 3>;

float clamp01(float v) { return std::clamp(v, 0.0f, 1.0f); }
float luma(const Vec3 &c) { return 0.2126f * c[0] + 0.7152f * c[1] + 0.0722f * c[2]; }
float mixf(float a, float b, float t) { return a + (b - a) * t; }

/** Curva em S suave (contraste) centrada em 0,5. */
float sCurve(float x, float amount)
{
    const float s = x * x * (3 - 2 * x); // smoothstep
    return mixf(x, s, amount);
}

Vec3 saturate(const Vec3 &c, float s)
{
    const float l = luma(c);
    return {l + (c[0] - l) * s, l + (c[1] - l) * s, l + (c[2] - l) * s};
}

Vec3 contrast(const Vec3 &c, float amount)
{
    return {sCurve(clamp01(c[0]), amount), sCurve(clamp01(c[1]), amount), sCurve(clamp01(c[2]), amount)};
}

/** Pretos levantados / brancos rebaixados (aspecto "fosco"). */
Vec3 fade(const Vec3 &c, float black, float white)
{
    return {black + c[0] * (white - black), black + c[1] * (white - black), black + c[2] * (white - black)};
}

/** Tinge sombras e realces com cores diferentes (split toning). */
Vec3 splitTone(const Vec3 &c, const Vec3 &shadow, const Vec3 &highlight, float amount)
{
    const float l = luma(c);
    const float ws = (1 - l) * (1 - l) * amount;
    const float wh = l * l * amount;
    Vec3 out;
    for (int i = 0; i < 3; ++i)
        out[i] = c[i] + (shadow[i] - 0.5f) * ws + (highlight[i] - 0.5f) * wh;
    return out;
}

using FilterFn = std::function<Vec3(Vec3)>;

struct FilterDef {
    BuiltinFilter info;
    FilterFn fn;
};

const std::vector<FilterDef> &filterDefs()
{
    static const std::vector<FilterDef> defs{
        {{"vivid", QString::fromUtf8(QT_TRANSLATE_NOOP("Filters", "Vivid")), QStringLiteral("color")},
         [](Vec3 c) { return contrast(saturate(c, 1.35f), 0.25f); }},
        {{"warm", QString::fromUtf8(QT_TRANSLATE_NOOP("Filters", "Warm")), QStringLiteral("color")},
         [](Vec3 c) { return saturate(Vec3{c[0] * 1.08f + 0.02f, c[1] * 1.02f + 0.01f, c[2] * 0.88f}, 1.05f); }},
        {{"cool", QString::fromUtf8(QT_TRANSLATE_NOOP("Filters", "Cool")), QStringLiteral("color")},
         [](Vec3 c) { return Vec3{c[0] * 0.9f, c[1] * 1.0f + 0.01f, c[2] * 1.1f + 0.03f}; }},
        {{"pastel", QString::fromUtf8(QT_TRANSLATE_NOOP("Filters", "Pastel")), QStringLiteral("color")},
         [](Vec3 c) { return fade(saturate(c, 0.75f), 0.08f, 0.98f); }},
        {{"golden", QString::fromUtf8(QT_TRANSLATE_NOOP("Filters", "Golden")), QStringLiteral("color")},
         [](Vec3 c) { return splitTone(contrast(c, 0.15f), {0.45f, 0.42f, 0.5f}, {0.85f, 0.65f, 0.3f}, 0.6f); }},
        {{"sunset", QString::fromUtf8(QT_TRANSLATE_NOOP("Filters", "Sunset")), QStringLiteral("color")},
         [](Vec3 c) { return splitTone(c, {0.55f, 0.35f, 0.6f}, {0.95f, 0.55f, 0.35f}, 0.7f); }},
        {{"tealorange", QString::fromUtf8(QT_TRANSLATE_NOOP("Filters", "Teal & Orange")), QStringLiteral("cinema")},
         [](Vec3 c) { return contrast(splitTone(c, {0.2f, 0.55f, 0.65f}, {0.9f, 0.6f, 0.35f}, 0.8f), 0.2f); }},
        {{"film", QString::fromUtf8(QT_TRANSLATE_NOOP("Filters", "Film")), QStringLiteral("cinema")},
         [](Vec3 c) { return fade(contrast(splitTone(c, {0.42f, 0.55f, 0.5f}, {0.6f, 0.55f, 0.45f}, 0.5f), 0.3f), 0.04f, 0.96f); }},
        {{"moody", QString::fromUtf8(QT_TRANSLATE_NOOP("Filters", "Moody")), QStringLiteral("cinema")},
         [](Vec3 c) {
             Vec3 d = saturate(c, 0.7f);
             d = splitTone(d, {0.35f, 0.45f, 0.6f}, {0.55f, 0.52f, 0.48f}, 0.6f);
             return contrast(Vec3{d[0] * 0.9f, d[1] * 0.9f, d[2] * 0.92f}, 0.35f);
         }},
        {{"matte", QString::fromUtf8(QT_TRANSLATE_NOOP("Filters", "Matte")), QStringLiteral("cinema")},
         [](Vec3 c) { return fade(saturate(c, 0.85f), 0.1f, 0.92f); }},
        {{"vintage", QString::fromUtf8(QT_TRANSLATE_NOOP("Filters", "Vintage")), QStringLiteral("retro")},
         [](Vec3 c) {
             Vec3 d = saturate(c, 0.65f);
             d = Vec3{d[0] * 1.05f + 0.03f, d[1] * 0.98f + 0.02f, d[2] * 0.82f + 0.05f};
             return fade(d, 0.07f, 0.93f);
         }},
        {{"sepia", QString::fromUtf8(QT_TRANSLATE_NOOP("Filters", "Sepia")), QStringLiteral("retro")},
         [](Vec3 c) {
             const float l = luma(c);
             return Vec3{l * 1.07f + 0.05f, l * 0.88f + 0.03f, l * 0.68f + 0.02f};
         }},
        {{"faded", QString::fromUtf8(QT_TRANSLATE_NOOP("Filters", "Faded")), QStringLiteral("retro")},
         [](Vec3 c) { return fade(saturate(c, 0.55f), 0.14f, 0.9f); }},
        {{"bw", QString::fromUtf8(QT_TRANSLATE_NOOP("Filters", "B&W")), QStringLiteral("bw")},
         [](Vec3 c) {
             const float l = luma(c);
             return Vec3{l, l, l};
         }},
        {{"noir", QString::fromUtf8(QT_TRANSLATE_NOOP("Filters", "Noir")), QStringLiteral("bw")},
         [](Vec3 c) {
             const float l = sCurve(sCurve(clamp01(luma(c)), 1.0f), 0.6f);
             return Vec3{l, l, l};
         }},
        {{"silver", QString::fromUtf8(QT_TRANSLATE_NOOP("Filters", "Silver")), QStringLiteral("bw")},
         [](Vec3 c) {
             const float l = luma(c);
             return fade(Vec3{l * 0.97f, l, l * 1.03f}, 0.06f, 0.97f);
         }},
    };
    return defs;
}

Lut3D lutFromFunction(const FilterFn &fn, int n, const QString &title)
{
    Lut3D lut;
    lut.size = n;
    lut.title = title;
    lut.rgb.resize(static_cast<size_t>(n) * n * n * 3);
    size_t i = 0;
    for (int b = 0; b < n; ++b)
        for (int g = 0; g < n; ++g)
            for (int r = 0; r < n; ++r) {
                const Vec3 out = fn({static_cast<float>(r) / (n - 1), static_cast<float>(g) / (n - 1),
                                     static_cast<float>(b) / (n - 1)});
                lut.rgb[i++] = clamp01(out[0]);
                lut.rgb[i++] = clamp01(out[1]);
                lut.rgb[i++] = clamp01(out[2]);
            }
    return lut;
}

} // namespace

std::array<float, 3> Lut3D::sample(float r, float g, float b) const
{
    const int n = size;
    auto coord = [&](float v, int axis) {
        const float d = domainMax[axis] - domainMin[axis];
        const float t = d > 0 ? (v - domainMin[axis]) / d : v;
        return clamp01(t) * (n - 1);
    };
    const float fr = coord(r, 0), fg = coord(g, 1), fb = coord(b, 2);
    const int r0 = std::min(static_cast<int>(fr), n - 2), g0 = std::min(static_cast<int>(fg), n - 2),
              b0 = std::min(static_cast<int>(fb), n - 2);
    const float tr = fr - r0, tg = fg - g0, tb = fb - b0;
    auto at = [&](int ri, int gi, int bi, int ch) { return rgb[((static_cast<size_t>(bi) * n + gi) * n + ri) * 3 + ch]; };
    std::array<float, 3> out{};
    for (int ch = 0; ch < 3; ++ch) {
        const float c00 = mixf(at(r0, g0, b0, ch), at(r0 + 1, g0, b0, ch), tr);
        const float c10 = mixf(at(r0, g0 + 1, b0, ch), at(r0 + 1, g0 + 1, b0, ch), tr);
        const float c01 = mixf(at(r0, g0, b0 + 1, ch), at(r0 + 1, g0, b0 + 1, ch), tr);
        const float c11 = mixf(at(r0, g0 + 1, b0 + 1, ch), at(r0 + 1, g0 + 1, b0 + 1, ch), tr);
        out[ch] = mixf(mixf(c00, c10, tg), mixf(c01, c11, tg), tb);
    }
    return out;
}

Lut3D parseCube(const QByteArray &data, QString *error)
{
    auto fail = [&](const QString &m) {
        if (error)
            *error = m;
        return Lut3D{};
    };
    Lut3D lut;
    int size1d = 0;
    std::vector<float> values;
    QTextStream in(data);
    while (!in.atEnd()) {
        const QString line = in.readLine().trimmed();
        if (line.isEmpty() || line.startsWith(QLatin1Char('#')))
            continue;
        const QStringList parts = line.split(QLatin1Char(' '), Qt::SkipEmptyParts);
        const QString key = parts.first().toUpper();
        if (key == QLatin1String("TITLE")) {
            lut.title = line.mid(5).trimmed().remove(QLatin1Char('"'));
        } else if (key == QLatin1String("LUT_3D_SIZE") && parts.size() >= 2) {
            lut.size = parts[1].toInt();
        } else if (key == QLatin1String("LUT_1D_SIZE") && parts.size() >= 2) {
            size1d = parts[1].toInt();
        } else if (key == QLatin1String("DOMAIN_MIN") && parts.size() >= 4) {
            lut.domainMin = {parts[1].toFloat(), parts[2].toFloat(), parts[3].toFloat()};
        } else if (key == QLatin1String("DOMAIN_MAX") && parts.size() >= 4) {
            lut.domainMax = {parts[1].toFloat(), parts[2].toFloat(), parts[3].toFloat()};
        } else if (key.at(0).isDigit() || key.at(0) == QLatin1Char('-') || key.at(0) == QLatin1Char('.')) {
            if (parts.size() < 3)
                return fail(QCoreApplication::translate("Core", "Invalid data line in .cube: %1").arg(line));
            for (int i = 0; i < 3; ++i) {
                bool ok = false;
                values.push_back(parts[i].toFloat(&ok));
                if (!ok)
                    return fail(QCoreApplication::translate("Core", "Invalid number in .cube: %1").arg(parts[i]));
            }
        }
        // Outras palavras-chave (LUT_3D_INPUT_RANGE etc.) são ignoradas.
    }
    if (lut.size == 0 && size1d >= 2) {
        // LUT 1D: aplica por canal numa tabela 3D de 33³.
        if (values.size() != static_cast<size_t>(size1d) * 3)
            return fail(QCoreApplication::translate("Core", "Entry count does not match LUT_1D_SIZE"));
        auto curve = [&](float v, int ch) {
            const float f = clamp01(v) * (size1d - 1);
            const int i0 = std::min(static_cast<int>(f), size1d - 2);
            return mixf(values[i0 * 3 + ch], values[(i0 + 1) * 3 + ch], f - i0);
        };
        Lut3D out = lutFromFunction([&](Vec3 c) { return Vec3{curve(c[0], 0), curve(c[1], 1), curve(c[2], 2)}; }, 33,
                                    lut.title);
        return out;
    }
    if (lut.size < 2 || lut.size > 256)
        return fail(QCoreApplication::translate("Core", "LUT_3D_SIZE missing or invalid"));
    if (values.size() != static_cast<size_t>(lut.size) * lut.size * lut.size * 3)
        return fail(QCoreApplication::translate("Core", "The .cube has %1 entries; expected %2³").arg(values.size() / 3).arg(lut.size));
    lut.rgb = std::move(values);
    return lut;
}

Lut3D loadCube(const QString &path, QString *error)
{
    QFile f(path);
    if (!f.open(QIODevice::ReadOnly)) {
        if (error)
            *error = f.errorString();
        return {};
    }
    return parseCube(f.readAll(), error);
}

QByteArray toCube(const Lut3D &lut)
{
    QByteArray out;
    if (!lut.title.isEmpty())
        out += "TITLE \"" + lut.title.toUtf8() + "\"\n";
    out += "LUT_3D_SIZE " + QByteArray::number(lut.size) + "\n";
    for (size_t i = 0; i < lut.rgb.size(); i += 3)
        out += QByteArray::number(lut.rgb[i], 'f', 6) + ' ' + QByteArray::number(lut.rgb[i + 1], 'f', 6) + ' ' +
               QByteArray::number(lut.rgb[i + 2], 'f', 6) + '\n';
    return out;
}

const std::vector<BuiltinFilter> &builtinFilters()
{
    static const std::vector<BuiltinFilter> list = [] {
        std::vector<BuiltinFilter> l;
        for (const auto &d : filterDefs())
            l.push_back(d.info);
        return l;
    }();
    return list;
}

const BuiltinFilter *findBuiltinFilter(const QString &id)
{
    for (const auto &f : builtinFilters())
        if (f.id == id)
            return &f;
    return nullptr;
}

Lut3D builtinFilterLut(const QString &id, int size)
{
    for (const auto &d : filterDefs())
        if (d.info.id == id)
            return lutFromFunction(d.fn, size, d.info.name);
    return {};
}

std::shared_ptr<const Lut3D> lutForKey(const QString &key)
{
    if (key.isEmpty())
        return nullptr;
    static QMutex mutex;
    static QHash<QString, std::shared_ptr<const Lut3D>> cache;
    QMutexLocker lock(&mutex);
    if (auto it = cache.find(key); it != cache.end())
        return *it;
    Lut3D lut = findBuiltinFilter(key) ? builtinFilterLut(key) : loadCube(key);
    std::shared_ptr<const Lut3D> ptr = lut.isValid() ? std::make_shared<const Lut3D>(std::move(lut)) : nullptr;
    cache.insert(key, ptr);
    return ptr;
}

QImage applyLut(const QImage &src, const Lut3D &lut, float intensity)
{
    QImage img = src.convertToFormat(QImage::Format_RGBA8888);
    if (!lut.isValid())
        return img;
    for (int y = 0; y < img.height(); ++y) {
        uchar *p = img.scanLine(y);
        for (int x = 0; x < img.width(); ++x, p += 4) {
            const auto o = lut.sample(p[0] / 255.f, p[1] / 255.f, p[2] / 255.f);
            for (int ch = 0; ch < 3; ++ch)
                p[ch] = static_cast<uchar>(std::lround(clamp01(mixf(p[ch] / 255.f, o[ch], intensity)) * 255));
        }
    }
    return img;
}

} // namespace clv

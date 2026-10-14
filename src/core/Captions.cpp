// SPDX-License-Identifier: GPL-3.0-or-later
#include "Captions.h"

#include <QRegularExpression>
#include <QStringList>

#include <algorithm>

namespace clv {

namespace {

// 00:01:02,345 (SRT) ou 00:01:02.345 / 01:02.345 (WebVTT)
const QRegularExpression kTimeLine(QStringLiteral(
    R"(^\s*((?:\d+:)?\d{1,2}:\d{2}[,.]\d{1,3})\s*-->\s*((?:\d+:)?\d{1,2}:\d{2}[,.]\d{1,3}))"));

Micros parseTimestamp(QString s)
{
    s.replace(QLatin1Char(','), QLatin1Char('.'));
    const QStringList parts = s.split(QLatin1Char(':'));
    double secs = 0;
    for (const QString &p : parts)
        secs = secs * 60 + p.toDouble();
    return static_cast<Micros>(secs * kSecond + 0.5);
}

QString formatSrtTime(Micros t)
{
    t = std::max<Micros>(0, t);
    const Micros ms = t / 1000;
    return QStringLiteral("%1:%2:%3,%4")
        .arg(ms / 3'600'000, 2, 10, QLatin1Char('0'))
        .arg(ms / 60'000 % 60, 2, 10, QLatin1Char('0'))
        .arg(ms / 1000 % 60, 2, 10, QLatin1Char('0'))
        .arg(ms % 1000, 3, 10, QLatin1Char('0'));
}

QString stripMarkup(QString s)
{
    static const QRegularExpression tags(QStringLiteral("<[^>]*>|\\{\\\\[^}]*\\}"));
    s.remove(tags);
    return s.trimmed();
}

} // namespace

std::vector<CaptionSegment> parseSubtitles(const QString &content)
{
    std::vector<CaptionSegment> out;
    QString text = content;
    text.replace(QStringLiteral("\r\n"), QStringLiteral("\n"));
    text.replace(QLatin1Char('\r'), QLatin1Char('\n'));
    const QStringList lines = text.split(QLatin1Char('\n'));

    for (int i = 0; i < lines.size(); ++i) {
        const auto m = kTimeLine.match(lines[i]);
        if (!m.hasMatch())
            continue;
        CaptionSegment seg;
        seg.start = parseTimestamp(m.captured(1));
        seg.end = parseTimestamp(m.captured(2));
        QStringList body;
        for (++i; i < lines.size() && !lines[i].trimmed().isEmpty(); ++i) {
            if (kTimeLine.match(lines[i]).hasMatch()) {
                --i; // bloco sem linha em branco: próximo cue
                break;
            }
            body << stripMarkup(lines[i]);
        }
        seg.text = body.join(QLatin1Char('\n')).trimmed();
        if (seg.end > seg.start && !seg.text.isEmpty())
            out.push_back(std::move(seg));
    }
    std::sort(out.begin(), out.end(), [](const auto &a, const auto &b) { return a.start < b.start; });
    return out;
}

QString toSrt(const std::vector<CaptionSegment> &segments)
{
    QString out;
    int n = 1;
    for (const auto &s : segments) {
        out += QString::number(n++) + QLatin1Char('\n');
        out += formatSrtTime(s.start) + QStringLiteral(" --> ") + formatSrtTime(s.end) + QLatin1Char('\n');
        out += s.text + QStringLiteral("\n\n");
    }
    return out;
}

std::vector<CaptionSegment> captionsFromTrack(const Track &track)
{
    std::vector<CaptionSegment> out;
    for (const Clip &c : track.clips)
        if (c.isText())
            out.push_back({c.start, c.end(), c.text});
    return out;
}

TextStyle captionStyle()
{
    TextStyle s;
    s.fontSize = 54;
    s.bold = true;
    s.color = QStringLiteral("#ffffff");
    s.strokeColor = QStringLiteral("#000000");
    s.strokeWidth = 3;
    s.background.clear();
    s.shadow = true;
    s.align = 1;
    return s;
}

Transform captionTransform()
{
    Transform t;
    t.y = 0.72;
    return t;
}

int addCaptionTrack(Project &p, const std::vector<CaptionSegment> &segments, const QString &name)
{
    Track track = createTrack(TrackKind::Text, name.isEmpty() ? QStringLiteral("Legendas") : name);
    track.captions = true;
    const Micros minLen = p.frameDuration();
    for (size_t i = 0; i < segments.size(); ++i) {
        const auto &s = segments[i];
        Micros end = s.end;
        if (i + 1 < segments.size())
            end = std::min(end, segments[i + 1].start);
        const Micros start = p.snapToFrame(std::max<Micros>(0, s.start));
        end = p.snapToFrame(end);
        if (end - start < minLen || s.text.trimmed().isEmpty())
            continue;
        if (!track.clips.empty() && start < track.clips.back().end())
            continue;
        Clip c = createTextClip(s.text.trimmed(), start, end - start);
        c.style = captionStyle();
        c.transform = captionTransform();
        track.clips.push_back(std::move(c));
    }
    p.tracks.push_back(std::move(track));
    return static_cast<int>(p.tracks.size()) - 1;
}

std::vector<CaptionSegment> reflowCaptions(const std::vector<CaptionSegment> &segments, int maxChars)
{
    std::vector<CaptionSegment> out;
    for (const auto &s : segments) {
        const QString flat = s.text.simplified();
        if (flat.size() <= maxChars) {
            out.push_back({s.start, s.end, flat});
            continue;
        }
        // Agrupa palavras em linhas de até maxChars.
        QStringList chunks;
        QString cur;
        for (const QString &w : flat.split(QLatin1Char(' '), Qt::SkipEmptyParts)) {
            if (!cur.isEmpty() && cur.size() + 1 + w.size() > maxChars) {
                chunks << cur;
                cur.clear();
            }
            cur += (cur.isEmpty() ? QString() : QStringLiteral(" ")) + w;
        }
        if (!cur.isEmpty())
            chunks << cur;
        const double total = flat.size();
        Micros t = s.start;
        double consumed = 0;
        for (int i = 0; i < chunks.size(); ++i) {
            consumed += chunks[i].size() + (i + 1 < chunks.size() ? 1 : 0);
            const Micros end = i + 1 == chunks.size() ? s.end
                                                      : s.start + static_cast<Micros>((s.end - s.start) * consumed / total);
            out.push_back({t, end, chunks[i]});
            t = end;
        }
    }
    return out;
}

} // namespace clv

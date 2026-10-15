// SPDX-License-Identifier: GPL-3.0-or-later
#include "Silence.h"

#include "Transcriber.h"

#include <algorithm>
#include <cmath>

namespace clv::engine {

namespace {
constexpr int kRate = 16000;
constexpr int kWindow = kRate / 50; // 20 ms
} // namespace

std::vector<std::pair<Micros, Micros>> detectSilencesPcm16k(const std::vector<float> &pcm, Micros origin,
                                                            const SilenceOptions &opts)
{
    std::vector<std::pair<Micros, Micros>> out;
    const double threshold = std::pow(10.0, opts.thresholdDb / 20.0);
    const auto windows = static_cast<std::int64_t>(pcm.size() / kWindow);
    auto windowTime = [&](std::int64_t w) { return origin + w * kWindow * kSecond / kRate; };
    std::int64_t runStart = -1;
    auto close = [&](std::int64_t endWindow) {
        const Micros a = windowTime(runStart) + (runStart == 0 ? 0 : opts.padding);
        const Micros b = windowTime(endWindow) - (endWindow == windows ? 0 : opts.padding);
        // A duração mínima vale para a pausa inteira; o respiro só reduz o que é cortado.
        if (windowTime(endWindow) - windowTime(runStart) >= opts.minSilence && b > a)
            out.emplace_back(a, b);
        runStart = -1;
    };
    for (std::int64_t w = 0; w < windows; ++w) {
        double acc = 0;
        for (int i = 0; i < kWindow; ++i) {
            const float x = pcm[static_cast<size_t>(w * kWindow + i)];
            acc += x * x;
        }
        const bool quiet = std::sqrt(acc / kWindow) < threshold;
        if (quiet && runStart < 0)
            runStart = w;
        else if (!quiet && runStart >= 0)
            close(w);
    }
    if (runStart >= 0)
        close(windows);
    return out;
}

std::vector<std::pair<Micros, Micros>> detectSilences(const Project &p, const SilenceOptions &opts,
                                                      const std::function<bool(double)> &progress)
{
    const Micros end = opts.rangeEnd < 0 ? p.duration() : std::min(opts.rangeEnd, p.duration());
    const Micros start = std::clamp<Micros>(opts.rangeStart, 0, end);
    if (end <= start)
        return {};
    const std::vector<float> pcm = renderProjectAudio16k(p, -1, start, end, progress);
    return detectSilencesPcm16k(pcm, start, opts);
}

} // namespace clv::engine

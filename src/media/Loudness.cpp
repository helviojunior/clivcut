// SPDX-License-Identifier: GPL-3.0-or-later
#include "Loudness.h"

#include "AudioDecoder.h"

#include <algorithm>
#include <cmath>
#include <limits>

namespace clv::media {

namespace {
constexpr int kHop = kSampleRate / 10; // 100 ms
constexpr int kHopsPerBlock = 4;        // bloco de 400 ms
} // namespace

void LoudnessMeter::add(const float *in, int frames)
{
    for (int i = 0; i < frames; ++i) {
        double energy = 0;
        for (int c = 0; c < 2; ++c) {
            const double x = in[i * 2 + c];
            m_peak = std::max(m_peak, std::abs(x));
            const double y = m_ch[c].highpass.process(m_ch[c].shelf.process(x));
            energy += y * y; // L e R com peso 1,0
        }
        m_acc += energy;
        if (++m_accFrames == kHop) {
            m_hopEnergy.push_back(m_acc);
            m_acc = 0;
            m_accFrames = 0;
        }
    }
}

double LoudnessMeter::integrated() const
{
    // Loudness de cada bloco de 400 ms (passo de 100 ms).
    std::vector<double> blocks;
    for (size_t i = 0; i + kHopsPerBlock <= m_hopEnergy.size(); ++i) {
        double sum = 0;
        for (int k = 0; k < kHopsPerBlock; ++k)
            sum += m_hopEnergy[i + k];
        const double meanSquare = sum / (kHop * kHopsPerBlock);
        blocks.push_back(meanSquare > 0 ? -0.691 + 10 * std::log10(meanSquare) : -std::numeric_limits<double>::infinity());
    }
    auto gatedMean = [&](double gate) {
        double acc = 0;
        int n = 0;
        for (double l : blocks)
            if (l > gate) {
                acc += std::pow(10.0, (l + 0.691) / 10);
                ++n;
            }
        return n > 0 ? -0.691 + 10 * std::log10(acc / n) : -std::numeric_limits<double>::infinity();
    };
    const double absolute = gatedMean(-70.0);
    if (!std::isfinite(absolute))
        return absolute;
    return gatedMean(std::max(-70.0, absolute - 10.0));
}

double measureLoudness(const QString &path, Micros start, Micros duration, const std::function<bool(double)> &progress,
                       double *peak)
{
    AudioDecoder dec;
    if (!dec.open(path))
        return std::numeric_limits<double>::quiet_NaN();
    if (duration <= 0)
        duration = std::max<Micros>(0, dec.duration() - start);
    LoudnessMeter meter;
    constexpr int kChunk = kSampleRate / 2;
    std::vector<float> buf(static_cast<size_t>(kChunk) * kChannels);
    const std::int64_t total = av_rescale(duration, kSampleRate, kSecond);
    for (std::int64_t done = 0; done < total;) {
        const int n = static_cast<int>(std::min<std::int64_t>(kChunk, total - done));
        dec.read(start + av_rescale(done, kSecond, kSampleRate), buf.data(), n);
        meter.add(buf.data(), n);
        done += n;
        if (progress && !progress(static_cast<double>(done) / total))
            return std::numeric_limits<double>::quiet_NaN();
    }
    if (peak)
        *peak = meter.peak();
    return meter.integrated();
}

} // namespace clv::media

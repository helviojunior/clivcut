// SPDX-License-Identifier: GPL-3.0-or-later
#include "AudioOutput.h"

#include "engine/AudioMixer.h"
#include "media/AudioDecoder.h"

#include <SDL.h>

#include <QDebug>

#include <algorithm>
#include <cstring>

using clv::media::kChannels;
using clv::media::kSampleRate;

namespace {
constexpr int kRingFrames = kSampleRate / 2; // 500 ms
constexpr int kAheadFrames = kSampleRate / 5; // produz até 200 ms à frente
constexpr int kChunkFrames = 1024;
} // namespace

AudioOutput::AudioOutput()
{
    m_ring.resize(static_cast<size_t>(kRingFrames) * kChannels);
    if (SDL_InitSubSystem(SDL_INIT_AUDIO) != 0) {
        qWarning() << "Audio unavailable:" << SDL_GetError();
        return;
    }
    SDL_AudioSpec want{};
    want.freq = kSampleRate;
    want.format = AUDIO_F32SYS;
    want.channels = kChannels;
    want.samples = 1024;
    want.callback = &AudioOutput::callback;
    want.userdata = this;
    SDL_AudioSpec have{};
    m_device = SDL_OpenAudioDevice(nullptr, 0, &want, &have, 0);
    if (m_device == 0) {
        qWarning() << "Could not open the audio device:" << SDL_GetError();
        return;
    }
    m_deviceLatencyFrames = have.samples;
    m_thread = std::thread(&AudioOutput::producer, this);
}

AudioOutput::~AudioOutput()
{
    m_quit = true;
    m_cv.notify_all();
    if (m_thread.joinable())
        m_thread.join();
    if (m_device)
        SDL_CloseAudioDevice(m_device);
    SDL_QuitSubSystem(SDL_INIT_AUDIO);
}

void AudioOutput::start(std::shared_ptr<const clv::Project> project, clv::Micros from)
{
    if (!m_device)
        return;
    SDL_PauseAudioDevice(m_device, 1);
    {
        std::lock_guard lock(m_mutex);
        m_project = std::move(project);
        m_from = from;
        SDL_LockAudioDevice(m_device);
        m_written = 0;
        m_read = 0;
        SDL_UnlockAudioDevice(m_device);
        ++m_generation;
        m_running = true;
    }
    m_cv.notify_all();
    SDL_PauseAudioDevice(m_device, 0);
}

void AudioOutput::stop()
{
    if (!m_device)
        return;
    m_running = false;
    SDL_PauseAudioDevice(m_device, 1);
}

void AudioOutput::updateProject(std::shared_ptr<const clv::Project> project)
{
    std::lock_guard lock(m_mutex);
    m_project = std::move(project);
}

clv::Micros AudioOutput::position() const
{
    const std::int64_t frames = std::max<std::int64_t>(0, m_read.load() - m_deviceLatencyFrames);
    return m_from + frames * clv::kSecond / kSampleRate;
}

void AudioOutput::callback(void *user, unsigned char *stream, int len)
{
    auto *self = static_cast<AudioOutput *>(user);
    auto *out = reinterpret_cast<float *>(stream);
    const int frames = len / static_cast<int>(sizeof(float) * kChannels);
    const std::int64_t read = self->m_read.load();
    const std::int64_t avail = self->m_written.load() - read;
    const int n = static_cast<int>(std::min<std::int64_t>(avail, frames));
    for (int i = 0; i < n; ++i) {
        const auto idx = static_cast<size_t>((read + i) % kRingFrames) * kChannels;
        out[i * 2] = self->m_ring[idx];
        out[i * 2 + 1] = self->m_ring[idx + 1];
    }
    std::memset(out + n * kChannels, 0, sizeof(float) * (frames - n) * kChannels);
    // Avança mesmo em underrun para o relógio não travar.
    self->m_read = read + frames;
    if (n < frames)
        self->m_written = std::max(self->m_written.load(), read + frames);
    self->m_cv.notify_all();
}

void AudioOutput::producer()
{
    clv::engine::AudioMixer mixer;
    std::vector<float> chunk(static_cast<size_t>(kChunkFrames) * kChannels);
    while (!m_quit) {
        std::shared_ptr<const clv::Project> project;
        clv::Micros from = 0;
        std::uint64_t gen = 0;
        {
            std::unique_lock lock(m_mutex);
            m_cv.wait_for(lock, std::chrono::milliseconds(10), [&] {
                return m_quit || (m_running && m_written.load() - m_read.load() < kAheadFrames);
            });
            if (m_quit)
                break;
            if (!m_running || m_written.load() - m_read.load() >= kAheadFrames)
                continue;
            project = m_project;
            from = m_from;
            gen = m_generation;
        }
        if (!project)
            continue;
        const std::int64_t w = m_written.load();
        const clv::Micros t = from + w * clv::kSecond / kSampleRate;
        mixer.mix(*project, t, chunk.data(), kChunkFrames);
        if (gen != m_generation)
            continue; // houve seek enquanto mixava
        SDL_LockAudioDevice(m_device);
        if (gen == m_generation && m_written.load() == w) {
            for (int i = 0; i < kChunkFrames; ++i) {
                const auto idx = static_cast<size_t>((w + i) % kRingFrames) * kChannels;
                m_ring[idx] = chunk[i * 2];
                m_ring[idx + 1] = chunk[i * 2 + 1];
            }
            m_written = w + kChunkFrames;
        }
        SDL_UnlockAudioDevice(m_device);
    }
}

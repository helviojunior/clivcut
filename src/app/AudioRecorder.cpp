// SPDX-License-Identifier: GPL-3.0-or-later
#include "AudioRecorder.h"

#include "media/AudioDecoder.h"

#include <SDL.h>

#include <QCoreApplication>
#include <QDir>
#include <QFileInfo>

#include <algorithm>
#include <cmath>

using clv::media::kChannels;
using clv::media::kSampleRate;

AudioRecorder::AudioRecorder()
{
    m_sdlReady = SDL_InitSubSystem(SDL_INIT_AUDIO) == 0;
}

AudioRecorder::~AudioRecorder()
{
    stop();
    if (m_sdlReady)
        SDL_QuitSubSystem(SDL_INIT_AUDIO);
}

QStringList AudioRecorder::devices()
{
    QStringList out;
    if (SDL_InitSubSystem(SDL_INIT_AUDIO) != 0)
        return out;
    const int n = SDL_GetNumAudioDevices(1);
    for (int i = 0; i < n; ++i)
        if (const char *name = SDL_GetAudioDeviceName(i, 1))
            out << QString::fromUtf8(name);
    SDL_QuitSubSystem(SDL_INIT_AUDIO);
    return out;
}

bool AudioRecorder::start(const QString &wavPath, const QString &device, QString *error)
{
    auto fail = [&](const QString &m) {
        if (error)
            *error = m;
        return false;
    };
    stop();
    if (!m_sdlReady)
        return fail(QString::fromUtf8(SDL_GetError()));
    QDir().mkpath(QFileInfo(wavPath).absolutePath());
    m_file.setFileName(wavPath);
    if (!m_file.open(QIODevice::WriteOnly | QIODevice::Truncate))
        return fail(m_file.errorString());
    m_frames = 0;
    m_peak = 0.f;
    m_pending.clear();
    writeHeader();

    SDL_AudioSpec want{};
    want.freq = kSampleRate;
    want.format = AUDIO_F32SYS;
    want.channels = kChannels; // microfone mono vira estéreo (o SDL converte)
    want.samples = 1024;
    want.callback = &AudioRecorder::callback;
    want.userdata = this;
    SDL_AudioSpec have{};
    const QByteArray name = device.toUtf8();
    m_device = SDL_OpenAudioDevice(device.isEmpty() ? nullptr : name.constData(), 1, &want, &have, 0);
    if (m_device == 0) {
        m_file.close();
        m_file.remove();
        return fail(QCoreApplication::translate("AudioRecorder", "Could not open the microphone: %1")
                        .arg(QString::fromUtf8(SDL_GetError())));
    }
    SDL_PauseAudioDevice(m_device, 0);
    return true;
}

void AudioRecorder::callback(void *userdata, std::uint8_t *stream, int len)
{
    auto *self = static_cast<AudioRecorder *>(userdata);
    const auto *in = reinterpret_cast<const float *>(stream);
    const int n = len / static_cast<int>(sizeof(float));
    float peak = 0;
    for (int i = 0; i < n; ++i)
        peak = std::max(peak, std::abs(in[i]));
    float old = self->m_peak.load();
    while (peak > old && !self->m_peak.compare_exchange_weak(old, peak)) {
    }
    std::lock_guard lock(self->m_mutex);
    self->m_pending.insert(self->m_pending.end(), in, in + n);
}

void AudioRecorder::flush()
{
    std::vector<float> chunk;
    {
        std::lock_guard lock(m_mutex);
        chunk.swap(m_pending);
    }
    if (chunk.empty() || !m_file.isOpen())
        return;
    m_file.write(reinterpret_cast<const char *>(chunk.data()), static_cast<qint64>(chunk.size() * sizeof(float)));
    m_frames += static_cast<std::int64_t>(chunk.size() / kChannels);
}

std::int64_t AudioRecorder::stop()
{
    if (m_device) {
        SDL_CloseAudioDevice(m_device); // depois disso o callback não roda mais
        m_device = 0;
    }
    if (!m_file.isOpen())
        return 0;
    flush();
    writeHeader();
    m_file.close();
    return m_frames.load();
}

void AudioRecorder::writeHeader()
{
    // Cabeçalho WAV (float 32 bits) com o tamanho atual; reescrito ao parar.
    const auto dataBytes = static_cast<quint32>(std::min<std::int64_t>(m_frames.load() * kChannels * 4, 0xFFFFFFF0LL));
    QByteArray h;
    auto u32 = [&](quint32 v) { h.append(reinterpret_cast<const char *>(&v), 4); };
    auto u16 = [&](quint16 v) { h.append(reinterpret_cast<const char *>(&v), 2); };
    h.append("RIFF");
    u32(36 + dataBytes);
    h.append("WAVEfmt ");
    u32(16);
    u16(3);
    u16(kChannels);
    u32(kSampleRate);
    u32(kSampleRate * kChannels * 4);
    u16(kChannels * 4);
    u16(32);
    h.append("data");
    u32(dataBytes);
    const qint64 pos = m_file.pos();
    m_file.seek(0);
    m_file.write(h);
    if (pos > h.size())
        m_file.seek(pos);
}

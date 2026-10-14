// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once

#include "core/Model.h"

#include <atomic>
#include <condition_variable>
#include <memory>
#include <mutex>
#include <thread>
#include <vector>

/**
 * Saída de áudio da reprodução (SDL2: PipeWire/Pulse/ALSA, WASAPI, CoreAudio).
 *
 * Uma thread produtora mixa o projeto à frente num buffer circular; o callback
 * do SDL consome. A posição de reprodução vem das amostras consumidas, e é o
 * relógio mestre do preview (vídeo segue o áudio).
 */
class AudioOutput {
public:
    AudioOutput();
    ~AudioOutput();

    bool available() const { return m_device != 0; }
    void start(std::shared_ptr<const clv::Project> project, clv::Micros from);
    void stop();
    void updateProject(std::shared_ptr<const clv::Project> project);
    /** Posição (µs da timeline) do que está sendo ouvido agora. */
    clv::Micros position() const;

private:
    static void callback(void *user, unsigned char *stream, int len);
    void producer();

    unsigned m_device = 0;
    int m_deviceLatencyFrames = 0;

    std::vector<float> m_ring;
    std::atomic<std::int64_t> m_written{0}; ///< quadros escritos no anel
    std::atomic<std::int64_t> m_read{0};    ///< quadros consumidos pelo dispositivo

    std::mutex m_mutex;
    std::condition_variable m_cv;
    std::shared_ptr<const clv::Project> m_project;
    clv::Micros m_from = 0;
    std::atomic<bool> m_running{false};
    std::atomic<bool> m_quit{false};
    std::atomic<std::uint64_t> m_generation{0};
    std::thread m_thread;
};

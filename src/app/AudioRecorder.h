// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once

#include <QFile>
#include <QString>
#include <QStringList>

#include <atomic>
#include <cstdint>
#include <mutex>
#include <vector>

/**
 * Gravação do microfone (SDL2) para um WAV float estéreo 48 kHz. O callback do
 * SDL só copia as amostras; flush() grava em disco (chamado por um timer na GUI).
 */
class AudioRecorder {
public:
    AudioRecorder();
    ~AudioRecorder();
    AudioRecorder(const AudioRecorder &) = delete;
    AudioRecorder &operator=(const AudioRecorder &) = delete;

    /** Dispositivos de entrada disponíveis (nomes do SDL). */
    static QStringList devices();

    /** `device` vazio = entrada padrão do sistema. */
    bool start(const QString &wavPath, const QString &device, QString *error);
    /** Para, grava o que falta e fecha o arquivo. Retorna os quadros gravados. */
    std::int64_t stop();
    void flush();
    bool recording() const { return m_device != 0; }
    std::int64_t frames() const { return m_frames.load(); }
    /** Pico desde a última leitura (0..1), para o medidor de nível. */
    float takePeak() { return m_peak.exchange(0.f); }

private:
    static void callback(void *userdata, std::uint8_t *stream, int len);
    void writeHeader();

    std::uint32_t m_device = 0;
    QFile m_file;
    std::mutex m_mutex;
    std::vector<float> m_pending;
    std::atomic<std::int64_t> m_frames{0};
    std::atomic<float> m_peak{0.f};
    bool m_sdlReady = false;
};

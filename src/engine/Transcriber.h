// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once

#include "core/Captions.h"
#include "core/Model.h"

#include <QString>

#include <functional>
#include <vector>

namespace clv::engine {

struct TranscribeOptions {
    QString modelPath;            ///< arquivo ggml do whisper (ex.: ggml-base.bin)
    QString language = QStringLiteral("auto"); ///< código ISO ("pt", "en"...) ou "auto"
    bool translateToEnglish = false;
    int maxChars = 42;            ///< tamanho máximo de cada legenda (0 = frases inteiras)
    int threads = 0;              ///< 0 = automático
    int trackIndex = -1;          ///< -1 = mixagem de todas as faixas com áudio
    Micros rangeStart = 0;
    Micros rangeEnd = -1;         ///< -1 = até o fim do projeto
};

/** true se este build tem suporte a transcrição (whisper.cpp). */
bool transcriptionAvailable();

/**
 * Transcreve o áudio do projeto (a mixagem que se ouve na timeline) com o
 * whisper.cpp, localmente. Tempos dos segmentos são da timeline.
 * `progress(fração)` retornando false cancela. Bloqueante: use numa thread.
 */
bool transcribeProject(const Project &p, const TranscribeOptions &opts, const std::function<bool(double)> &progress,
                       std::vector<CaptionSegment> &out, QString *error);

/** Transcreve diretamente amostras mono 16 kHz (usado pelos testes e pela CLI). */
bool transcribePcm16k(const std::vector<float> &pcm, const TranscribeOptions &opts,
                      const std::function<bool(double)> &progress, std::vector<CaptionSegment> &out, QString *error);

/** Áudio mono 16 kHz do projeto no intervalo pedido (mixagem da timeline). */
std::vector<float> renderProjectAudio16k(const Project &p, int trackIndex, Micros start, Micros end,
                                         const std::function<bool(double)> &progress = {});

} // namespace clv::engine

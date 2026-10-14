// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once

#include "core/Model.h"
#include "media/Encoder.h"

#include <QString>

#include <functional>

namespace clv::render {
class OffscreenRhi;
}

namespace clv::engine {

struct ExportOptions {
    QString outputPath;
    media::EncodeSettings encode; ///< largura/altura/fps; 0 = do projeto
    Micros rangeStart = 0;
    Micros rangeEnd = -1; ///< -1 = fim do projeto
};

/**
 * Renderiza o projeto quadro a quadro com o mesmo compositor do preview
 * (GPU offscreen) e codifica com FFmpeg. Bloqueante: rode numa thread.
 *
 * `progress(fração)` é chamado a cada quadro; retornar false cancela.
 */
class Exporter {
public:
    using Progress = std::function<bool(double)>;

    /** `rhi` opcional: um OffscreenRhi já criado (obrigatório fora da thread da GUI com OpenGL). */
    bool run(const Project &p, ExportOptions opts, const Progress &progress, QString *error,
             render::OffscreenRhi *rhi = nullptr);
};

} // namespace clv::engine

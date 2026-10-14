// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once

#include "core/Model.h"

#include <QSize>
#include <QString>

#include <functional>
#include <vector>

namespace clv::render {
class OffscreenRhi;
}

/**
 * Cache de renderização da pré-visualização: a timeline é dividida em trechos
 * de 2 s e cada trecho pode ser pré-renderizado num vídeo H.264 só de
 * keyframes. O nome do arquivo é um hash de tudo o que afeta a imagem do
 * trecho (clipes, efeitos, mídia e canvas), então qualquer edição invalida
 * apenas os trechos afetados. Só a imagem é cacheada; o áudio continua ao vivo.
 */
namespace clv::engine {

constexpr Micros kRenderChunk = 2 * kSecond;

/** Pasta do cache (~/.cache/clivcut/render; CLIVCUT_CACHE_DIR muda a base). */
QString renderCacheDir();

/** Número de trechos da timeline. */
int renderChunkCount(const Project &p);

/** Hash do conteúdo visual de cada trecho (independente da resolução). */
std::vector<QString> renderChunkKeys(const Project &p);

/** Tamanho dos quadros cacheados: canvas com o lado maior limitado a `longSide`. */
QSize renderCacheSize(const Project &p, int longSide);

/** Arquivo de um trecho para uma chave e um tamanho de pré-visualização. */
QString renderCacheFile(const QString &key, int longSide);

/**
 * Renderiza os trechos `chunks` (índices) que ainda não estão no cache.
 * `progress(fração)` retornando false cancela; `chunkDone(índice)` avisa cada trecho pronto.
 */
bool renderChunks(const Project &p, const std::vector<int> &chunks, int longSide, render::OffscreenRhi *rhi,
                  const std::function<bool(double)> &progress, const std::function<void(int)> &chunkDone,
                  QString *error);

/** Estado do cache para a pré-visualização: arquivo de cada trecho (vazio = não renderizado). */
struct RenderCacheView {
    int longSide = 0;
    std::vector<QString> files;
    QString fileAt(Micros t) const
    {
        const auto i = static_cast<size_t>(t / kRenderChunk);
        return t >= 0 && i < files.size() ? files[i] : QString();
    }
};

/** Apaga todo o cache de renderização. Devolve os bytes liberados. */
qint64 clearRenderCache();

} // namespace clv::engine

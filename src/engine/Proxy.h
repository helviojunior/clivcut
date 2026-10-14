// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once

#include "core/Model.h"

#include <QSize>
#include <QString>

#include <functional>

/**
 * Proxies: cópias leves (H.264, ≤960 px, keyframes frequentes) de mídias
 * pesadas, usadas só na pré-visualização. A exportação sempre lê o original.
 */
namespace clv::engine {

constexpr int kProxyMaxSide = 960;

/** Pasta de cache dos proxies (~/.cache/clivcut/proxies; CLIVCUT_CACHE_DIR muda a base). */
QString proxyCacheDir();

/** Caminho do proxy para um arquivo (muda se o arquivo original mudar). */
QString proxyPathFor(const QString &mediaPath);

/** Vídeos maiores que 1080p (ou com mais de 60 fps) se beneficiam de proxy. */
bool proxyRecommended(const MediaAsset &asset);

/**
 * Gera o proxy de um vídeo. `progress(fração)` retornando false cancela.
 * Escreve num arquivo temporário e só publica ao terminar.
 */
bool generateProxy(const QString &mediaPath, const QString &outPath, const std::function<bool(double)> &progress,
                   QString *error);

/**
 * Registro (seguro entre threads) de proxies prontos: caminho original →
 * caminho do proxy. Consultado pela pré-visualização.
 */
void registerProxy(const QString &mediaPath, const QString &proxyPath);
void unregisterProxy(const QString &mediaPath);
QString registeredProxy(const QString &mediaPath);

/** Registra o proxy se já existir no cache. Retorna true se existe. */
bool discoverProxy(const QString &mediaPath);

} // namespace clv::engine

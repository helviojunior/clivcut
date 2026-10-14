// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once

#include <QQuickAsyncImageProvider>

/**
 * image://thumb/<assetId>/<tempoµs>  → miniatura de vídeo/imagem
 * image://wave/<assetId>/<inµs>/<durµs>/<cor> → forma de onda do trecho
 *
 * Geração em thread pool com cache em memória.
 */
class ThumbnailProvider : public QQuickAsyncImageProvider {
public:
    QQuickImageResponse *requestImageResponse(const QString &id, const QSize &requestedSize) override;
};

/** image://filter/<filtro>/<assetId>/<tempoµs> ou image://filter/<filtro>/sample */
class FilterPreviewProvider : public QQuickAsyncImageProvider {
public:
    QQuickImageResponse *requestImageResponse(const QString &id, const QSize &requestedSize) override;
};

class WaveformProvider : public QQuickAsyncImageProvider {
public:
    QQuickImageResponse *requestImageResponse(const QString &id, const QSize &requestedSize) override;
};

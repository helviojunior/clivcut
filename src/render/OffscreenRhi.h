// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once

#include <QString>

#include <rhi/qrhi.h>

#include <memory>

class QOffscreenSurface;
class QSurfaceFormat;
class QVulkanInstance;

// QT_CONFIG(vulkan) não garante os headers do Vulkan no build do app (ex.: Qt do Homebrew);
// sem eles o Qt esconde o QVulkanInstance.
#if QT_CONFIG(vulkan) && __has_include(<vulkan/vulkan.h>)
#define CLIVCUT_HAS_VULKAN 1
#endif

namespace clv::render {

/**
 * Um QRhi sem janela (export e testes). Windows: D3D11; macOS: Metal; depois
 * Vulkan e OpenGL. CLIVCUT_RHI=d3d11|metal|vulkan|opengl força um backend.
 * Deve ser criado na thread da GUI (o QOffscreenSurface exige isso), mas pode
 * ser usado depois a partir de uma única outra thread via moveToThread lógico.
 */
class OffscreenRhi {
public:
    OffscreenRhi();
    ~OffscreenRhi();

    /**
     * `fallbackSurface`: superfície criada na thread da GUI (createFallbackSurface)
     * para permitir OpenGL quando o OffscreenRhi é criado noutra thread.
     */
    bool create(QString *error = nullptr, QOffscreenSurface *fallbackSurface = nullptr);

    /** Cria (na thread da GUI) uma superfície para uso posterior noutra thread. */
    static QOffscreenSurface *createFallbackSurface();
    /** Formato OpenGL usado (3.3 Core ou ES 3.0). */
    static QSurfaceFormat glFormat();
    QRhi *rhi() const { return m_rhi.get(); }
    QString backendName() const;

private:
#ifdef CLIVCUT_HAS_VULKAN
    std::unique_ptr<QVulkanInstance> m_vulkan;
#endif
    std::unique_ptr<QOffscreenSurface> m_surface;
    std::unique_ptr<QRhi> m_rhi;
};

} // namespace clv::render

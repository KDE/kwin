/*
    KWin - the KDE window manager
    This file is part of the KDE project.

    SPDX-FileCopyrightText: 2022 Xaver Hugl <xaver.hugl@gmail.com>

    SPDX-License-Identifier: GPL-2.0-or-later
*/
#include "drm_virtual_egl_layer.h"
#include "drm_egl_backend.h"
#include "drm_gpu.h"
#include "drm_logging.h"
#include "drm_virtual_output.h"
#include "opengl/eglnativefence.h"
#include "opengl/eglswapchain.h"
#include "opengl/glrendertimequery.h"
#include "scene/surfaceitem_wayland.h"
#include "wayland/surface.h"

#include <QRegion>
#include <drm_fourcc.h>
#include <errno.h>
#include <gbm.h>
#include <unistd.h>

namespace KWin
{

VirtualEglGbmLayer::VirtualEglGbmLayer(EglGbmBackend *eglBackend, DrmVirtualOutput *output)
    : m_output(output)
    , m_eglBackend(eglBackend)
{
}

VirtualEglGbmLayer::~VirtualEglGbmLayer() = default;

std::optional<OutputLayerBeginFrameInfo> VirtualEglGbmLayer::beginFrame()
{
    // gbm surface
    if (doesGbmSwapchainFit(m_gbmSwapchain.get())) {
        m_oldGbmSwapchain.reset();
        m_oldDamageJournal.clear();
    } else {
        if (doesGbmSwapchainFit(m_oldGbmSwapchain.get())) {
            m_gbmSwapchain = m_oldGbmSwapchain;
            m_damageJournal = m_oldDamageJournal;
        } else {
            if (const auto swapchain = createGbmSwapchain()) {
                m_oldGbmSwapchain = m_gbmSwapchain;
                m_oldDamageJournal = m_damageJournal;
                m_gbmSwapchain = swapchain;
                m_damageJournal = DamageJournal();
            } else {
                return std::nullopt;
            }
        }
    }

    if (!m_eglBackend->openglContext()->makeCurrent()) {
        return std::nullopt;
    }

    auto slot = m_gbmSwapchain->acquire();
    if (!slot) {
        return std::nullopt;
    }

    m_currentSlot = slot;

    if (!m_query) {
        m_query = std::make_unique<GLRenderTimeQuery>();
    }
    m_query->begin();

    const QRegion repair = m_damageJournal.accumulate(slot->age(), infiniteRegion());
    return OutputLayerBeginFrameInfo{
        .renderTarget = RenderTarget(slot->framebuffer()),
        .repaint = repair,
    };
}

bool VirtualEglGbmLayer::endFrame(const QRegion &renderedRegion, const QRegion &damagedRegion)
{
    m_query->end();
    glFlush();
    m_currentDamage = damagedRegion;
    m_damageJournal.add(damagedRegion);

    EGLNativeFence releaseFence{m_eglBackend->eglDisplayObject()};
    m_gbmSwapchain->release(m_currentSlot, releaseFence.takeFileDescriptor());
    return true;
}

QRegion VirtualEglGbmLayer::currentDamage() const
{
    return m_currentDamage;
}

std::shared_ptr<EglSwapchain> VirtualEglGbmLayer::createGbmSwapchain() const
{
    static bool modifiersEnvSet = false;
    static const bool modifiersEnv = qEnvironmentVariableIntValue("KWIN_DRM_USE_MODIFIERS", &modifiersEnvSet) != 0;
    const bool allowModifiers = !modifiersEnvSet || modifiersEnv;

    const auto tranches = m_eglBackend->tranches();
    for (const auto &tranche : tranches) {
        for (auto it = tranche.formatTable.constBegin(); it != tranche.formatTable.constEnd(); it++) {
            const auto size = m_output->modeSize();
            const auto format = it.key();
            const auto modifiers = it.value();

            if (allowModifiers && !modifiers.isEmpty()) {
                if (auto swapchain = EglSwapchain::create(m_eglBackend->gpu()->drmDevice()->allocator(), m_eglBackend->openglContext(), size, format, modifiers)) {
                    return swapchain;
                }
            }

            static const QList<uint64_t> implicitModifier{DRM_FORMAT_MOD_INVALID};
            if (auto swapchain = EglSwapchain::create(m_eglBackend->gpu()->drmDevice()->allocator(), m_eglBackend->openglContext(), size, format, implicitModifier)) {
                return swapchain;
            }
        }
    }
    qCWarning(KWIN_DRM) << "couldn't create a gbm swapchain for a virtual output!";
    return nullptr;
}

bool VirtualEglGbmLayer::doesGbmSwapchainFit(EglSwapchain *swapchain) const
{
    return swapchain && swapchain->size() == m_output->modeSize();
}

std::shared_ptr<GLTexture> VirtualEglGbmLayer::texture() const
{
    if (m_scanoutBuffer) {
        return m_eglBackend->importDmaBufAsTexture(*m_scanoutBuffer->dmabufAttributes());
    } else if (m_currentSlot) {
        return m_currentSlot->texture();
    }
    return nullptr;
}

bool VirtualEglGbmLayer::doAttemptScanout(GraphicsBuffer *buffer, const QRectF &sourceRect, const QSizeF &targetSize, OutputTransform transform, const ColorDescription &color, const QRegion &damage)
{
    static bool valid;
    static const bool directScanoutDisabled = qEnvironmentVariableIntValue("KWIN_DRM_NO_DIRECT_SCANOUT", &valid) == 1 && valid;
    if (directScanoutDisabled) {
        return false;
    }

    if (sourceRect != QRectF(QPointF(0, 0), targetSize) || targetSize != m_output->modeSize() || targetSize != buffer->size()) {
        return false;
    }
    m_scanoutBuffer = buffer;
    m_currentDamage = damage;
    m_scanoutColor = color;
    return true;
}

void VirtualEglGbmLayer::releaseBuffers()
{
    m_eglBackend->openglContext()->makeCurrent();
    m_gbmSwapchain.reset();
    m_oldGbmSwapchain.reset();
    m_currentSlot.reset();
    if (m_scanoutBuffer) {
        m_scanoutBuffer->unref();
        m_scanoutBuffer = nullptr;
    }
}

std::chrono::nanoseconds VirtualEglGbmLayer::queryRenderTime() const
{
    m_eglBackend->makeCurrent();
    return m_query->result();
}

DrmDevice *VirtualEglGbmLayer::scanoutDevice() const
{
    return m_eglBackend->drmDevice();
}

QHash<uint32_t, QList<uint64_t>> VirtualEglGbmLayer::supportedDrmFormats() const
{
    return m_eglBackend->supportedFormats();
}

const ColorDescription &VirtualEglGbmLayer::colorDescription() const
{
    return m_scanoutBuffer ? m_scanoutColor : ColorDescription::sRGB;
}
}

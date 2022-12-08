/*
    KWin - the KDE window manager
    This file is part of the KDE project.

    SPDX-FileCopyrightText: 2022 Xaver Hugl <xaver.hugl@gmail.com>

    SPDX-License-Identifier: GPL-2.0-or-later
*/
#pragma once
#include "drm_layer.h"

#include <QMap>
#include <QPointer>
#include <QRegion>
#include <epoxy/egl.h>
#include <optional>

namespace KWaylandServer
{
class SurfaceInterface;
}

namespace KWin
{

class GbmSwapchain;
class GLTexture;
class EglGbmBackend;
class GbmBuffer;
class DrmVirtualOutput;

class VirtualEglGbmLayer : public DrmOutputLayer
{
public:
    VirtualEglGbmLayer(EglGbmBackend *eglBackend, DrmVirtualOutput *output);

    std::optional<OutputLayerBeginFrameInfo> beginFrame() override;
    bool endFrame(const QRegion &renderedRegion, const QRegion &damagedRegion) override;
    bool scanout(SurfaceItem *surfaceItem) override;

    QRegion currentDamage() const override;
    std::shared_ptr<GLTexture> texture() const override;
    void releaseBuffers() override;
    quint32 format() const override;

private:
    std::shared_ptr<GbmSwapchain> createGbmSwapchain() const;
    bool doesGbmSwapchainFit(GbmSwapchain *swapchain) const;

    QPointer<KWaylandServer::SurfaceInterface> m_scanoutSurface;
    std::shared_ptr<GbmBuffer> m_currentBuffer;
    std::shared_ptr<GLFramebuffer> m_fbo;
    std::shared_ptr<GLTexture> m_texture;
    QRegion m_currentDamage;
    std::shared_ptr<GbmSwapchain> m_gbmSwapchain;
    std::shared_ptr<GbmSwapchain> m_oldGbmSwapchain;

    DrmVirtualOutput *const m_output;
    EglGbmBackend *const m_eglBackend;
};

}

/*
    KWin - the KDE window manager
    This file is part of the KDE project.

    SPDX-FileCopyrightText: 2022 Xaver Hugl <xaver.hugl@gmail.com>

    SPDX-License-Identifier: GPL-2.0-or-later
*/
#pragma once
#include "drm_layer.h"
#include "utils/damagejournal.h"

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

class DrmEglSwapchain;
class DrmEglSwapchainSlot;
class GraphicsBuffer;
class GLTexture;
class EglGbmBackend;
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
    std::shared_ptr<DrmEglSwapchain> createGbmSwapchain() const;
    bool doesGbmSwapchainFit(DrmEglSwapchain *swapchain) const;

    QPointer<KWaylandServer::SurfaceInterface> m_scanoutSurface;
    QPointer<GraphicsBuffer> m_scanoutBuffer;
    DamageJournal m_damageJournal;
    DamageJournal m_oldDamageJournal;
    QRegion m_currentDamage;
    std::shared_ptr<DrmEglSwapchain> m_gbmSwapchain;
    std::shared_ptr<DrmEglSwapchain> m_oldGbmSwapchain;
    std::shared_ptr<DrmEglSwapchainSlot> m_currentSlot;

    DrmVirtualOutput *const m_output;
    EglGbmBackend *const m_eglBackend;
};

}

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

namespace KWin
{

class EglSwapchain;
class EglSwapchainSlot;
class GraphicsBuffer;
class GLTexture;
class EglGbmBackend;
class DrmVirtualOutput;
class GLRenderTimeQuery;
class SurfaceInterface;

class VirtualEglGbmLayer : public DrmOutputLayer
{
public:
    VirtualEglGbmLayer(EglGbmBackend *eglBackend, DrmVirtualOutput *output);
    ~VirtualEglGbmLayer() override;

    std::optional<OutputLayerBeginFrameInfo> beginFrame() override;
    bool endFrame(const QRegion &renderedRegion, const QRegion &damagedRegion) override;
    bool scanout(SurfaceItem *surfaceItem) override;

    QRegion currentDamage() const override;
    std::shared_ptr<GLTexture> texture() const override;
    void releaseBuffers() override;
    quint32 format() const override;
    std::chrono::nanoseconds queryRenderTime() const override;

private:
    std::shared_ptr<EglSwapchain> createGbmSwapchain() const;
    bool doesGbmSwapchainFit(EglSwapchain *swapchain) const;

    QPointer<SurfaceInterface> m_scanoutSurface;
    QPointer<GraphicsBuffer> m_scanoutBuffer;
    DamageJournal m_damageJournal;
    DamageJournal m_oldDamageJournal;
    QRegion m_currentDamage;
    std::shared_ptr<EglSwapchain> m_gbmSwapchain;
    std::shared_ptr<EglSwapchain> m_oldGbmSwapchain;
    std::shared_ptr<EglSwapchainSlot> m_currentSlot;
    std::unique_ptr<GLRenderTimeQuery> m_query;

    DrmVirtualOutput *const m_output;
    EglGbmBackend *const m_eglBackend;
};

}

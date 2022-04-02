/*
    KWin - the KDE window manager
    This file is part of the KDE project.

    SPDX-FileCopyrightText: 2022 Xaver Hugl <xaver.hugl@gmail.com>

    SPDX-License-Identifier: GPL-2.0-or-later
*/
#pragma once

#include "dmabuf_feedback.h"
#include "drm_layer.h"
#include "egl_gbm_layer_surface.h"

namespace KWin
{

class DrmGbmBuffer;

class EglGbmOverlayLayer : public DrmOverlayLayer
{
public:
    EglGbmOverlayLayer(EglGbmBackend *eglBackend, DrmPlane *overlayPlane, DrmPipeline *pipeline);
    ~EglGbmOverlayLayer();

    std::optional<QRegion> beginFrame(const QRect &geometry) override;
    void aboutToStartPainting(const QRegion &damagedRegion) override;
    void endFrame(const QRegion &renderedRegion, const QRegion &damagedRegion) override;
    bool scanout(SurfaceItem *surfaceItem) override;
    QSharedPointer<DrmBuffer> currentBuffer() const override;
    QRegion currentDamage() const override;
    QSharedPointer<GLTexture> texture() const override;
    QRect pixelGeometry() const override;
    QRect geometry() const override;
    void pageFlipped() override;

private:
    void destroyResources();

    QSharedPointer<DrmBuffer> m_currentBuffer;
    QRegion m_currentDamage;
    QRect m_geometry;
    QRect m_pixelGeometry;

    EglGbmLayerSurface m_surface;
    DmabufFeedback m_dmabufFeedback;
    DrmPlane *const m_plane;
    // TODO allow the pipeline to change
    DrmPipeline *const m_pipeline;
};

}

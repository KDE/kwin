/*
    KWin - the KDE window manager
    This file is part of the KDE project.

    SPDX-FileCopyrightText: 2022 Xaver Hugl <xaver.hugl@gmail.com>

    SPDX-License-Identifier: GPL-2.0-or-later
*/
#pragma once
#include "drm_layer.h"

#include "dmabuf_feedback.h"
#include "egl_gbm_layer_surface.h"

#include <QMap>
#include <QPointer>
#include <QRegion>
#include <QSharedPointer>
#include <epoxy/egl.h>
#include <optional>

namespace KWaylandServer
{
class SurfaceInterface;
class LinuxDmaBufV1ClientBuffer;
}

namespace KWin
{

class EglGbmBackend;
class DrmGbmBuffer;

class EglGbmLayer : public DrmPipelineLayer
{
public:
    EglGbmLayer(EglGbmBackend *eglbackend, DrmPipeline *pipeline);
    ~EglGbmLayer();

    std::optional<QRegion> beginFrame(const QRect &geometry) override;
    void aboutToStartPainting(const QRegion &damagedRegion) override;
    void endFrame(const QRegion &renderedRegion, const QRegion &damagedRegion) override;
    bool scanout(SurfaceItem *surfaceItem) override;
    QSharedPointer<DrmBuffer> testBuffer() override;
    QSharedPointer<DrmBuffer> currentBuffer() const override;
    bool hasDirectScanoutBuffer() const override;
    QRegion currentDamage() const override;
    QSharedPointer<GLTexture> texture() const override;
    QRect geometry() const override;

private:
    bool renderTestBuffer();
    void destroyResources();

    QSharedPointer<DrmGbmBuffer> m_scanoutBuffer;
    QSharedPointer<DrmBuffer> m_currentBuffer;
    QRegion m_currentDamage;

    EglGbmLayerSurface m_surface;
    DmabufFeedback m_dmabufFeedback;
};

}

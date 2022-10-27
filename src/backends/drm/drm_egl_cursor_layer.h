/*
    KWin - the KDE window manager
    This file is part of the KDE project.

    SPDX-FileCopyrightText: 2022 Xaver Hugl <xaver.hugl@gmail.com>

    SPDX-License-Identifier: GPL-2.0-or-later
*/
#pragma once
#include "drm_layer.h"

#include "drm_dmabuf_feedback.h"
#include "drm_egl_layer_surface.h"

#include <QMap>
#include <QPointer>
#include <QRegion>
#include <epoxy/egl.h>
#include <optional>

namespace KWin
{

class EglGbmBackend;
class DrmGbmBuffer;

class EglGbmCursorLayer : public DrmOverlayLayer
{
public:
    EglGbmCursorLayer(EglGbmBackend *eglBackend, DrmPipeline *pipeline);

    std::optional<OutputLayerBeginFrameInfo> beginFrame() override;
    void aboutToStartPainting(const QRegion &damagedRegion) override;
    bool endFrame(const QRegion &renderedRegion, const QRegion &damagedRegion) override;
    std::shared_ptr<DrmFramebuffer> currentBuffer() const override;
    QRegion currentDamage() const override;
    bool checkTestBuffer() override;
    void releaseBuffers() override;

private:
    EglGbmLayerSurface m_surface;
};

}

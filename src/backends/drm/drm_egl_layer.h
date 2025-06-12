/*
    KWin - the KDE window manager
    This file is part of the KDE project.

    SPDX-FileCopyrightText: 2022 Xaver Hugl <xaver.hugl@gmail.com>

    SPDX-License-Identifier: GPL-2.0-or-later
*/
#pragma once
#include "drm_layer.h"

#include "drm_egl_layer_surface.h"

#include <QMap>
#include <QPointer>
#include <QRegion>
#include <epoxy/egl.h>
#include <optional>

namespace KWin
{

class EglGbmBackend;
class DrmPlane;

class EglGbmLayer : public DrmPipelineLayer
{
public:
    explicit EglGbmLayer(EglGbmBackend *eglBackend, DrmPlane *plane);
    explicit EglGbmLayer(EglGbmBackend *eglBackend, DrmGpu *gpu, DrmPlane::TypeIndex type);

    std::optional<OutputLayerBeginFrameInfo> doBeginFrame() override;
    bool doEndFrame(const QRegion &renderedRegion, const QRegion &damagedRegion, OutputFrame *frame) override;
    bool preparePresentationTest() override;
    std::shared_ptr<DrmFramebuffer> currentBuffer() const override;
    void releaseBuffers() override;

private:
    bool importScanoutBuffer(GraphicsBuffer *buffer, const std::shared_ptr<OutputFrame> &frame) override;

    EglGbmLayerSurface m_surface;
    std::shared_ptr<DrmFramebuffer> m_scanoutBuffer;
};

}

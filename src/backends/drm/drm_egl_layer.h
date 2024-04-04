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

class EglGbmLayer : public DrmPipelineLayer
{
public:
    EglGbmLayer(EglGbmBackend *eglBackend, DrmPipeline *pipeline);

    std::optional<OutputLayerBeginFrameInfo> beginFrame() override;
    bool endFrame(const QRegion &renderedRegion, const QRegion &damagedRegion) override;
    bool checkTestBuffer() override;
    std::shared_ptr<DrmFramebuffer> currentBuffer() const override;
    QRegion currentDamage() const override;
    std::shared_ptr<GLTexture> texture() const override;
    ColorDescription colorDescription() const;
    void releaseBuffers() override;
    std::chrono::nanoseconds queryRenderTime() const override;
    OutputTransform hardwareTransform() const override;
    QRect bufferSourceBox() const override;
    DrmDevice *scanoutDevice() const override;
    QHash<uint32_t, QList<uint64_t>> supportedDrmFormats() const override;

private:
    bool doAttemptScanout(GraphicsBuffer *buffer, const QRectF &sourceRect, const QSizeF &size, OutputTransform transform, const ColorDescription &color, const QRegion &damage) override;

    std::shared_ptr<DrmFramebuffer> m_scanoutBuffer;
    // the transform the drm plane will apply to the buffer
    OutputTransform m_scanoutTransform = OutputTransform::Kind::Normal;
    // the output transform the buffer is made for
    OutputTransform m_scanoutBufferTransform = OutputTransform::Kind::Normal;
    QRect m_bufferSourceBox;
    QRegion m_currentDamage;

    EglGbmLayerSurface m_surface;
};

}

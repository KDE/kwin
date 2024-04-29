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
    explicit EglGbmLayer(EglGbmBackend *eglBackend, DrmPipeline *pipeline, DrmPlane::TypeIndex type);

    std::optional<OutputLayerBeginFrameInfo> doBeginFrame() override;
    bool doEndFrame(const QRegion &renderedRegion, const QRegion &damagedRegion, OutputFrame *frame) override;
    bool checkTestBuffer() override;
    std::shared_ptr<DrmFramebuffer> currentBuffer() const override;
    std::shared_ptr<GLTexture> texture() const override;
    ColorDescription colorDescription() const;
    void releaseBuffers() override;
    DrmDevice *scanoutDevice() const override;
    QHash<uint32_t, QList<uint64_t>> supportedDrmFormats() const override;
    std::optional<QSize> fixedSize() const override;
    void notifyScanoutSuccessful() override;

private:
    bool doImportScanoutBuffer(GraphicsBuffer *buffer, const ColorDescription &color) override;

    std::shared_ptr<DrmFramebuffer> m_scanoutBuffer;

    EglGbmLayerSurface m_surface;
};

}

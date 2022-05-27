/*
    KWin - the KDE window manager
    This file is part of the KDE project.

    SPDX-FileCopyrightText: 2022 Xaver Hugl <xaver.hugl@gmail.com>

    SPDX-License-Identifier: GPL-2.0-or-later
*/
#pragma once
#include "drm_layer.h"

#include <QSharedPointer>

namespace KWin
{

class EglGbmBackend;

class DrmLeaseEglGbmLayer : public DrmPipelineLayer
{
public:
    DrmLeaseEglGbmLayer(DrmPipeline *pipeline);

    OutputLayerBeginFrameInfo beginFrame() override;
    bool endFrame(const QRegion &damagedRegion, const QRegion &renderedRegion) override;

    bool checkTestBuffer() override;
    std::shared_ptr<DrmFramebuffer> currentBuffer() const override;
    void releaseBuffers() override;

private:
    std::shared_ptr<DrmFramebuffer> m_framebuffer;
};

}

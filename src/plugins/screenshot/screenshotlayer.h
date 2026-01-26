/*
    SPDX-FileCopyrightText: 2025 Xaver Hugl <xaver.hugl@kde.org>

    SPDX-License-Identifier: GPL-2.0-or-later
*/
#pragma once
#include "core/outputlayer.h"

namespace KWin
{

class GLFramebuffer;

class ScreenshotLayer : public OutputLayer
{
public:
    explicit ScreenshotLayer(LogicalOutput *output, GLFramebuffer *buffer);

    DrmDevice *scanoutDevice() const override;
    FormatModifierMap supportedDrmFormats() const override;
    void releaseBuffers() override;

private:
    std::optional<OutputLayerBeginFrameInfo> doBeginFrame() override;
    bool doEndFrame(const Region &renderedRegion, const Region &damagedRegion, OutputFrame *frame) override;

    GLFramebuffer *const m_buffer;
};

}

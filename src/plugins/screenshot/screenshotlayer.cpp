/*
    SPDX-FileCopyrightText: 2025 Xaver Hugl <xaver.hugl@kde.org>

    SPDX-License-Identifier: GPL-2.0-or-later
*/
#include "screenshotlayer.h"
#include "opengl/eglcontext.h"
#include "opengl/glframebuffer.h"

namespace KWin
{

ScreenshotLayer::ScreenshotLayer(LogicalOutput *output, GLFramebuffer *buffer)
    : OutputLayer(output->backendOutput(), OutputLayerType::Primary)
    , m_buffer(buffer)
{
}

FormatModifierMap ScreenshotLayer::supportedDrmFormats() const
{
    return {};
}

std::optional<OutputLayerBeginFrameInfo> ScreenshotLayer::beginFrame(OutputFrame *frame)
{
    if (!m_buffer->context()->makeCurrent()) {
        return std::nullopt;
    }

    return OutputLayerBeginFrameInfo{
        .renderTarget = RenderTarget(m_buffer),
        .repaint = Region::infinite(),
    };
}

bool ScreenshotLayer::endFrame(const Region &renderedRegion, const Region &damagedRegion, OutputFrame *frame)
{
    return true;
}

void ScreenshotLayer::releaseBuffers()
{
}

}

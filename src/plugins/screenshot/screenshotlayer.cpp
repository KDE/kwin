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

DrmDevice *ScreenshotLayer::scanoutDevice() const
{
    return nullptr;
}

QHash<uint32_t, QList<uint64_t>> ScreenshotLayer::supportedDrmFormats() const
{
    return {};
}

std::optional<OutputLayerBeginFrameInfo> ScreenshotLayer::doBeginFrame()
{
    if (!m_buffer->context()->makeCurrent()) {
        return std::nullopt;
    }

    return OutputLayerBeginFrameInfo{
        .renderTarget = RenderTarget(m_buffer),
        .repaint = Region::infinite(),
    };
}

bool ScreenshotLayer::doEndFrame(const Region &renderedRegion, const Region &damagedRegion, OutputFrame *frame)
{
    return true;
}

void ScreenshotLayer::releaseBuffers()
{
}
}

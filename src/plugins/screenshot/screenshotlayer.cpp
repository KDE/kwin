/*
    SPDX-FileCopyrightText: 2025 Xaver Hugl <xaver.hugl@kde.org>

    SPDX-License-Identifier: GPL-2.0-or-later
*/
#include "screenshotlayer.h"

namespace KWin
{

ScreenshotLayer::ScreenshotLayer(Output *output, GLFramebuffer *buffer)
    : OutputLayer(output, OutputLayerType::Primary)
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
    return OutputLayerBeginFrameInfo{
        .renderTarget = RenderTarget(m_buffer),
        .repaint = infiniteRegion(),
    };
}

bool ScreenshotLayer::doEndFrame(const QRegion &renderedRegion, const QRegion &damagedRegion, OutputFrame *frame)
{
    return true;
}

void ScreenshotLayer::releaseBuffers()
{
}
}

/*
    SPDX-FileCopyrightText: 2025 Xaver Hugl <xaver.hugl@kde.org>

    SPDX-License-Identifier: GPL-2.0-or-later
*/
#include "zoomlayer.h"
#include "opengl/glframebuffer.h"

namespace KWin
{

ZoomLayer::ZoomLayer(LogicalOutput *output)
    : OutputLayer(output->backendOutput(), OutputLayerType::Primary)
{
    // prevent the layer from scheduling frames on the actual output
    setRenderLoop(nullptr);
}

void ZoomLayer::setFramebuffer(GLFramebuffer *buffer)
{
    // TODO is there a better way to deal with this?
    m_buffer = buffer;
}

DrmDevice *ZoomLayer::scanoutDevice() const
{
    return nullptr;
}

QHash<uint32_t, QList<uint64_t>> ZoomLayer::supportedDrmFormats() const
{
    return {};
}

std::optional<OutputLayerBeginFrameInfo> ZoomLayer::doBeginFrame()
{
    return OutputLayerBeginFrameInfo{
        .renderTarget = RenderTarget(m_buffer),
        .repaint = Rect(QPoint(), m_buffer->size()),
    };
}

bool ZoomLayer::doEndFrame(const Region &renderedRegion, const Region &damagedRegion, OutputFrame *frame)
{
    return true;
}

void ZoomLayer::releaseBuffers()
{
}
}

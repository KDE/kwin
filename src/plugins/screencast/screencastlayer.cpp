/*
    SPDX-FileCopyrightText: 2025 Xaver Hugl <xaver.hugl@kde.org>

    SPDX-License-Identifier: GPL-2.0-or-later
*/
#include "screencastlayer.h"

namespace KWin
{

ScreencastLayer::ScreencastLayer(LogicalOutput *output, const QHash<uint32_t, QList<uint64_t>> &formats)
    : OutputLayer(output->backendOutput(), OutputLayerType::Primary)
    , m_formats(formats)
{
    // prevent the layer from scheduling frames on the actual output
    setRenderLoop(nullptr);
}

void ScreencastLayer::setColor(const std::shared_ptr<ColorDescription> &color)
{
    if (color == m_color) {
        return;
    }
    m_color = color;
    addDeviceRepaint(Rect(QPoint(), m_targetRect.size()));
}

void ScreencastLayer::setFramebuffer(GLFramebuffer *buffer, const Region &bufferDamage)
{
    // TODO is there a better way to deal with this?
    m_buffer = buffer;
    m_bufferDamage = bufferDamage;
}

DrmDevice *ScreencastLayer::scanoutDevice() const
{
    return nullptr;
}

QHash<uint32_t, QList<uint64_t>> ScreencastLayer::supportedDrmFormats() const
{
    return m_formats;
}

std::optional<OutputLayerBeginFrameInfo> ScreencastLayer::doBeginFrame()
{
    return OutputLayerBeginFrameInfo{
        .renderTarget = RenderTarget(m_buffer, m_color),
        .repaint = m_bufferDamage,
    };
}

bool ScreencastLayer::doEndFrame(const Region &renderedRegion, const Region &damagedRegion, OutputFrame *frame)
{
    return true;
}

void ScreencastLayer::releaseBuffers()
{
}
}

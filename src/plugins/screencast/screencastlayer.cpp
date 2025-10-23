/*
    SPDX-FileCopyrightText: 2025 Xaver Hugl <xaver.hugl@kde.org>

    SPDX-License-Identifier: GPL-2.0-or-later
*/
#include "screencastlayer.h"

namespace KWin
{

ScreencastLayer::ScreencastLayer(Output *output, const QHash<uint32_t, QList<uint64_t>> &formats)
    : OutputLayer(output, OutputLayerType::Primary)
    , m_formats(formats)
{
}

void ScreencastLayer::setColor(const std::shared_ptr<ColorDescription> &color)
{
    if (color == m_color) {
        return;
    }
    m_color = color;
    addRepaint(QRect(QPoint(), m_sourceRect.toAlignedRect().size()));
}

void ScreencastLayer::setFramebuffer(GLFramebuffer *buffer, const QRegion &bufferDamage)
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

bool ScreencastLayer::doEndFrame(const QRegion &renderedRegion, const QRegion &damagedRegion, OutputFrame *frame)
{
    return true;
}

void ScreencastLayer::releaseBuffers()
{
}
}

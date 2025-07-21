/*
    SPDX-FileCopyrightText: 2025 Xaver Hugl <xaver.hugl@kde.org>

    SPDX-License-Identifier: GPL-2.0-or-later
*/
#include "screencastlayer.h"

namespace KWin
{

ScreencastLayer::ScreencastLayer(Output *output, const QHash<uint32_t, QList<uint64_t>> &formats)
    : OutputLayer(output)
    , m_formats(formats)
{
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
        .renderTarget = RenderTarget(m_buffer),
        .repaint = m_bufferDamage,
    };
}

bool ScreencastLayer::doEndFrame(const QRegion &renderedRegion, const QRegion &damagedRegion, OutputFrame *frame)
{
    return true;
}

}

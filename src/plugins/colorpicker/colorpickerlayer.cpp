/*
    SPDX-FileCopyrightText: 2025 Błażej Szczygieł <mumei6102@gmail.com>

    SPDX-License-Identifier: GPL-2.0-or-later
*/
#include "colorpickerlayer.h"

namespace KWin
{

ColorPickerLayer::ColorPickerLayer(BackendOutput *output, GLFramebuffer *buffer)
    : OutputLayer(output, OutputLayerType::Primary)
    , m_buffer(buffer)
{
}

DrmDevice *ColorPickerLayer::scanoutDevice() const
{
    return nullptr;
}

FormatModifierMap ColorPickerLayer::supportedDrmFormats() const
{
    return {};
}

std::optional<OutputLayerBeginFrameInfo> ColorPickerLayer::doBeginFrame()
{
    return OutputLayerBeginFrameInfo{
        .renderTarget = RenderTarget(m_buffer),
        .repaint = Region::infinite(),
    };
}

bool ColorPickerLayer::doEndFrame(const Region &renderedRegion, const Region &damagedRegion, OutputFrame *frame)
{
    return true;
}

void ColorPickerLayer::releaseBuffers()
{
}
}

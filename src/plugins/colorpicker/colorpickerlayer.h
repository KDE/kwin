/*
    SPDX-FileCopyrightText: 2025 Błażej Szczygieł <mumei6102@gmail.com>

    SPDX-License-Identifier: GPL-2.0-or-later
*/
#pragma once
#include "core/outputlayer.h"

namespace KWin
{

class GLFramebuffer;

class ColorPickerLayer : public OutputLayer
{
public:
    explicit ColorPickerLayer(BackendOutput *output, GLFramebuffer *buffer);

    FormatModifierMap supportedDrmFormats() const override;
    void releaseBuffers() override;

    std::optional<OutputLayerBeginFrameInfo> beginFrame() override;
    bool endFrame(const Region &renderedRegion, const Region &damagedRegion, OutputFrame *frame) override;

private:
    GLFramebuffer *const m_buffer;
};

}

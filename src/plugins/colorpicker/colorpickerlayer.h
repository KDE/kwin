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
    explicit ColorPickerLayer(Output *output, GLFramebuffer *buffer);

    DrmDevice *scanoutDevice() const override;
    QHash<uint32_t, QList<uint64_t>> supportedDrmFormats() const override;
    void releaseBuffers() override;

private:
    std::optional<OutputLayerBeginFrameInfo> doBeginFrame() override;
    bool doEndFrame(const QRegion &renderedRegion, const QRegion &damagedRegion, OutputFrame *frame) override;

    GLFramebuffer *const m_buffer;
};

}

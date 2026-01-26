/*
    SPDX-FileCopyrightText: 2025 Xaver Hugl <xaver.hugl@kde.org>

    SPDX-License-Identifier: GPL-2.0-or-later
*/
#pragma once
#include "core/outputlayer.h"

namespace KWin
{

class ScreencastLayer : public OutputLayer
{
public:
    explicit ScreencastLayer(LogicalOutput *output, const FormatModifierMap &formats);

    void setFramebuffer(GLFramebuffer *buffer, const Region &bufferDamage);

    DrmDevice *scanoutDevice() const override;
    FormatModifierMap supportedDrmFormats() const override;
    void releaseBuffers() override;

private:
    std::optional<OutputLayerBeginFrameInfo> doBeginFrame() override;
    bool doEndFrame(const Region &renderedRegion, const Region &damagedRegion, OutputFrame *frame) override;

    const FormatModifierMap m_formats;
    GLFramebuffer *m_buffer = nullptr;
    Region m_bufferDamage;
};

}

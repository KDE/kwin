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

    void setFramebuffer(GraphicsBuffer *buffer, const Region &bufferDamage);

    FormatModifierMap supportedDrmFormats() const override;
    void releaseBuffers() override;

    std::optional<OutputLayerBeginFrameInfo> beginFrame(OutputFrame *frame = nullptr) override;
    bool endFrame(const Region &damagedRegion, FileDescriptor &&sync, OutputFrame *frame) override;

private:
    const FormatModifierMap m_formats;
    GraphicsBufferRef m_buffer;
    Region m_bufferDamage;
};

}

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
    explicit ScreencastLayer(Output *output, const QHash<uint32_t, QList<uint64_t>> &formats);

    void setFramebuffer(GLFramebuffer *buffer, const QRegion &bufferDamage);

    DrmDevice *scanoutDevice() const override;
    QHash<uint32_t, QList<uint64_t>> supportedDrmFormats() const override;

private:
    std::optional<OutputLayerBeginFrameInfo> doBeginFrame() override;
    bool doEndFrame(const QRegion &renderedRegion, const QRegion &damagedRegion, OutputFrame *frame) override;

    const QHash<uint32_t, QList<uint64_t>> m_formats;
    GLFramebuffer *m_buffer = nullptr;
    QRegion m_bufferDamage;
};

}

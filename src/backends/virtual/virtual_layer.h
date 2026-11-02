/*
    KWin - the KDE window manager
    This file is part of the KDE project.

    SPDX-FileCopyrightText: 2015 Martin Gräßlin <mgraesslin@kde.org>
    SPDX-FileCopyrightText: 2026 Xaver Hugl <xaver.hugl@kde.org>

    SPDX-License-Identifier: GPL-2.0-or-later
*/
#pragma once
#include "core/outputlayer.h"
#include "utils/damagejournal.h"

namespace KWin
{

class RenderBackend;
class Swapchain;
class SwapchainSlot;

class KWIN_EXPORT VirtualLayer : public OutputLayer
{
public:
    VirtualLayer(BackendOutput *output, RenderBackend *backend);
    ~VirtualLayer() override;

    std::optional<OutputLayerBeginFrameInfo> beginFrame(OutputFrame *frame) override;
    bool endFrame(const Region &damagedDeviceRegion, FileDescriptor &&sync, OutputFrame *frame) override;

    FormatModifierMap supportedDrmFormats() const override;
    void releaseBuffers() override;

    QImage download() const;

private:
    RenderBackend *const m_backend;
    std::shared_ptr<Swapchain> m_swapchain;
    std::shared_ptr<SwapchainSlot> m_current;
    DamageJournal m_damageJournal;
};

}

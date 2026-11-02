/*
    KWin - the KDE window manager
    This file is part of the KDE project.

    SPDX-FileCopyrightText: 2015 Martin Gräßlin <mgraesslin@kde.org>
    SPDX-FileCopyrightText: 2026 Xaver Hugl <xaver.hugl@kde.org>

    SPDX-License-Identifier: GPL-2.0-or-later
*/
#pragma once
#include "core/outputlayer.h"

namespace KWin
{

class SwapchainSlot;
class Swapchain;
class X11WindowedBackend;
class X11WindowedOutput;
class X11WindowedEglBackend;
class RenderBackend;

class X11WindowedPrimaryLayer : public OutputLayer
{
public:
    X11WindowedPrimaryLayer(X11WindowedBackend *outputBackend, RenderBackend *renderBackend, X11WindowedOutput *output);
    ~X11WindowedPrimaryLayer() override;

    std::optional<OutputLayerBeginFrameInfo> beginFrame(OutputFrame *frame) override;
    bool endFrame(const Region &damagedDeviceRegion, FileDescriptor &&sync, OutputFrame *frame) override;
    FormatModifierMap supportedDrmFormats() const override;
    void releaseBuffers() override;

private:
    std::shared_ptr<Swapchain> m_swapchain;
    std::shared_ptr<SwapchainSlot> m_buffer;
    X11WindowedOutput *const m_output;
    X11WindowedBackend *const m_outputBackend;
    RenderBackend *const m_renderBackend;
};

class X11WindowedCursorLayer : public OutputLayer
{
    Q_OBJECT

public:
    X11WindowedCursorLayer(X11WindowedBackend *outputBackend, RenderBackend *renderBackend, X11WindowedOutput *output);
    ~X11WindowedCursorLayer() override;

    std::optional<OutputLayerBeginFrameInfo> beginFrame(OutputFrame *frame) override;
    bool endFrame(const Region &damagedDeviceRegion, FileDescriptor &&sync, OutputFrame *frame) override;
    FormatModifierMap supportedDrmFormats() const override;
    void releaseBuffers() override;

private:
    X11WindowedBackend *const m_outputBackend;
    RenderBackend *const m_renderBackend;
    std::unique_ptr<Swapchain> m_swapchain;
    std::shared_ptr<SwapchainSlot> m_slot;
};

}

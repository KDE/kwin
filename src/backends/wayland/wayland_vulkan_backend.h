/*
    KWin - the KDE window manager
    This file is part of the KDE project.

    SPDX-FileCopyrightText: 2026 Xaver Hugl <xaver.hugl@kde.org>

    SPDX-License-Identifier: GPL-2.0-or-later
*/
#pragma once
#include "utils/damagejournal.h"
#include "vulkan/vulkan_backend.h"
#include "wayland_layer.h"

namespace KWin
{

class VulkanSwapchain;
class VulkanSwapchainSlot;

namespace Wayland
{

class WaylandBackend;
class WaylandVulkanBackend;

class WaylandVulkanLayer : public WaylandLayer
{
public:
    WaylandVulkanLayer(WaylandOutput *output, WaylandVulkanBackend *backend, OutputLayerType type, int zpos);
    ~WaylandVulkanLayer() override;

    std::optional<OutputLayerBeginFrameInfo> doBeginFrame() override;
    bool doEndFrame(const Region &renderedDeviceRegion, const Region &damagedDeviceRegion, OutputFrame *frame) override;
    bool importScanoutBuffer(GraphicsBuffer *buffer, const std::shared_ptr<OutputFrame> &frame) override;
    DrmDevice *scanoutDevice() const override;
    FormatModifierMap supportedDrmFormats() const override;
    void releaseBuffers() override;

private:
    DamageJournal m_damageJournal;
    std::shared_ptr<VulkanSwapchain> m_swapchain;
    std::shared_ptr<VulkanSwapchainSlot> m_buffer;
    WaylandVulkanBackend *const m_backend;

    friend class WaylandEglBackend;
};

class WaylandVulkanBackend : public VulkanBackend
{
    Q_OBJECT

public:
    WaylandVulkanBackend(WaylandBackend *b);
    ~WaylandVulkanBackend() override;

    WaylandBackend *backend() const;
    DrmDevice *drmDevice() const override;
    FormatModifierMap supportedFormats() const override;
    QList<OutputLayer *> compatibleOutputLayers(BackendOutput *output) override;

private:
    void createOutputLayers(BackendOutput *output);

    WaylandBackend *const m_backend;
    FormatModifierMap m_formats;
};

}
}

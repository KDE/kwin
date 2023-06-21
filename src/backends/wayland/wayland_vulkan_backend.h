/*
    KWin - the KDE window manager
    This file is part of the KDE project.

    SPDX-FileCopyrightText: 2023 Xaver Hugl <xaver.hugl@gmail.com>

    SPDX-License-Identifier: GPL-2.0-or-later
*/
#pragma once
#include "core/graphicsbuffer.h"
#include "core/graphicsbufferallocator.h"
#include "core/outputlayer.h"
#include "platformsupport/scenes/vulkan/vulkan_backend.h"
#include "vulkan/vulkan_device.h"
#include "vulkan/vulkan_texture.h"
#include "wayland_backend.h"
#include "wayland_output.h"

namespace KWin
{
class VulkanSwapchain;
class VulkanSwapchainSlot;

namespace Wayland
{

class WaylandDisplay;
class WaylandVulkanLayer;

class WaylandVulkanBackend : public VulkanBackend
{
public:
    WaylandVulkanBackend(WaylandBackend *backend);
    ~WaylandVulkanBackend() override;

    bool init() override;
    bool testImportBuffer(GraphicsBuffer *buffer) override;
    QHash<uint32_t, QVector<uint64_t>> supportedFormats() const override;
    OutputLayer *primaryLayer(Output *output) override;
    bool present(Output *output, const std::shared_ptr<OutputFrame> &frame) override;
    WaylandBackend *backend() const;

private:
    VulkanDevice *findDevice() const;

    WaylandBackend *const m_backend;

    VulkanDevice *m_mainDevice = nullptr;
    std::map<Output *, std::unique_ptr<WaylandVulkanLayer>> m_outputs;
};

class WaylandVulkanLayer : public OutputLayer
{
public:
    explicit WaylandVulkanLayer(WaylandOutput *output, WaylandVulkanBackend *backend, VulkanDevice *device, GraphicsBufferAllocator *allocator, WaylandDisplay *display);
    ~WaylandVulkanLayer();

    std::optional<OutputLayerBeginFrameInfo> doBeginFrame() override;
    bool doEndFrame(const QRegion &renderedRegion, const QRegion &damagedRegion, OutputFrame *frame) override;
    DrmDevice *scanoutDevice() const override;
    QHash<uint32_t, QList<uint64_t>> supportedDrmFormats() const override;

    wl_buffer *currentBuffer() const;

private:
    WaylandVulkanBackend *const m_backend;
    WaylandOutput *const m_output;
    VulkanDevice *const m_device;
    GraphicsBufferAllocator *const m_allocator;
    WaylandDisplay *const m_display;

    std::shared_ptr<VulkanSwapchain> m_swapchain;
    std::shared_ptr<VulkanSwapchainSlot> m_currentSlot;
    wl_buffer *m_presentationBuffer = nullptr;
};

}
}

/*
    KWin - the KDE window manager
    This file is part of the KDE project.

    SPDX-FileCopyrightText: 2023-2026 Xaver Hugl <xaver.hugl@kde.org>

    SPDX-License-Identifier: GPL-2.0-or-later
*/
#pragma once
#include "core/drm_formats.h"
#include "core/graphicsbufferallocator.h"
#include "kwin_export.h"
#include "utils/filedescriptor.h"

#include <QPoint>
#include <QSize>
#include <memory>
#include <vector>
#include <vulkan/vulkan.h>

namespace KWin
{

class VulkanDevice;
class VulkanBuffer;
class VulkanBufferSwapchain;
class SyncReleasePoint;
class GraphicsBufferReleasePoint;
class HostMemoryGraphicsBufferAllocator;

class KWIN_EXPORT VulkanBufferSwapchainSlot
{
public:
    explicit VulkanBufferSwapchainSlot(GraphicsBuffer *buffer, std::shared_ptr<VulkanBuffer> &&vulkanBuffer);
    ~VulkanBufferSwapchainSlot();

    GraphicsBuffer *buffer() const;
    VulkanBuffer *vulkanBuffer() const;
    int age() const;
    bool isBusy() const;
    const FileDescriptor &releaseFd() const;
    std::shared_ptr<SyncReleasePoint> releasePoint();

private:
    GraphicsBuffer *const m_buffer;
    std::shared_ptr<VulkanBuffer> m_vulkanBuffer;
    int m_age = 0;
    std::shared_ptr<GraphicsBufferReleasePoint> m_releasePoint;
    friend class VulkanBufferSwapchain;
};

/**
 * Like VulkanSwapchain, but instead of textures, this imports each buffer as a VkBuffer.
 * This allows using them in more diverse ways, such as importing the same host memory
 * into two different drivers and using it to copy data from one to the other.
 */
class KWIN_EXPORT VulkanBufferSwapchain
{
public:
    explicit VulkanBufferSwapchain(VulkanDevice *device, const std::shared_ptr<HostMemoryGraphicsBufferAllocator> &allocator, const GraphicsBufferOptions &options, std::shared_ptr<VulkanBufferSwapchainSlot> &&initialSlot);
    ~VulkanBufferSwapchain();

    QSize size() const;
    uint32_t format() const;
    uint64_t modifier() const;
    bool scanout() const;

    std::shared_ptr<VulkanBufferSwapchainSlot> acquire();
    void release(VulkanBufferSwapchainSlot *slot, FileDescriptor &&releaseFd);

    void resetBufferAge();

    static std::unique_ptr<VulkanBufferSwapchain> create(VulkanDevice *device, const std::shared_ptr<HostMemoryGraphicsBufferAllocator> &allocator, const GraphicsBufferOptions &options);

private:
    VulkanDevice *const m_device;
    const std::shared_ptr<HostMemoryGraphicsBufferAllocator> m_allocator;
    const GraphicsBufferOptions m_options;
    std::vector<std::shared_ptr<VulkanBufferSwapchainSlot>> m_slots;
};

}

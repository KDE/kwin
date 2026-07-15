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

#include <QSize>
#include <memory>
#include <vector>
#include <vulkan/vulkan.h>

namespace KWin
{

class VulkanDevice;
class VulkanTexture;
class VulkanSwapchain;
class SyncReleasePoint;
class GraphicsBufferReleasePoint;

class KWIN_EXPORT VulkanSwapchainSlot
{
public:
    explicit VulkanSwapchainSlot(std::shared_ptr<GraphicsBuffer> &&buffer, std::shared_ptr<VulkanTexture> &&texture);
    ~VulkanSwapchainSlot();

    GraphicsBuffer *buffer() const;
    VulkanTexture *texture() const;
    int age() const;
    bool isBusy() const;
    const FileDescriptor &releaseFd() const;
    std::shared_ptr<SyncReleasePoint> releasePoint();

private:
    std::shared_ptr<GraphicsBuffer> m_buffer;
    std::shared_ptr<VulkanTexture> m_texture;
    int m_age = 0;
    std::shared_ptr<GraphicsBufferReleasePoint> m_releasePoint;
    friend class VulkanSwapchain;
};

class KWIN_EXPORT VulkanSwapchain
{
public:
    explicit VulkanSwapchain(VulkanDevice *device, GraphicsBufferAllocator *allocator, const GraphicsBufferOptions &options, std::shared_ptr<VulkanSwapchainSlot> &&initialSlot);
    ~VulkanSwapchain();

    QSize size() const;
    uint32_t format() const;
    uint64_t modifier() const;
    bool scanout() const;

    std::shared_ptr<VulkanSwapchainSlot> acquire();
    void release(VulkanSwapchainSlot *slot, FileDescriptor &&releaseFd);

    void resetBufferAge();

    static std::unique_ptr<VulkanSwapchain> create(VulkanDevice *device, GraphicsBufferAllocator *allocator, GraphicsBufferOptions options);

private:
    VulkanDevice *const m_device;
    GraphicsBufferAllocator *const m_allocator;
    const GraphicsBufferOptions m_options;
    std::vector<std::shared_ptr<VulkanSwapchainSlot>> m_slots;
};

}

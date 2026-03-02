/*
    KWin - the KDE window manager
    This file is part of the KDE project.

    SPDX-FileCopyrightText: 2023-2026 Xaver Hugl <xaver.hugl@kde.org>

    SPDX-License-Identifier: GPL-2.0-or-later
*/
#pragma once
#include "core/drm_formats.h"
#include "kwin_export.h"
#include "utils/filedescriptor.h"

#include <QSize>
#include <memory>
#include <vector>
#include <vulkan/vulkan.h>

namespace KWin
{

class GraphicsBuffer;
class GraphicsBufferAllocator;
class VulkanDevice;
class VulkanTexture;
class VulkanSwapchain;

class KWIN_EXPORT VulkanSwapchainSlot
{
public:
    explicit VulkanSwapchainSlot(GraphicsBuffer *buffer, std::unique_ptr<VulkanTexture> &&texture);
    ~VulkanSwapchainSlot();

    GraphicsBuffer *buffer() const;
    VulkanTexture *texture() const;
    int age() const;
    bool isBusy() const;

private:
    GraphicsBuffer *const m_buffer;
    std::unique_ptr<VulkanTexture> m_texture;
    int m_age = 0;
    FileDescriptor m_releaseFd;
    friend class VulkanSwapchain;
};

class KWIN_EXPORT VulkanSwapchain
{
public:
    explicit VulkanSwapchain(VulkanDevice *device, GraphicsBufferAllocator *allocator, const QSize &size, uint32_t format, uint64_t modifier, const std::shared_ptr<VulkanSwapchainSlot> &initialSlot);
    ~VulkanSwapchain();

    QSize size() const;
    uint32_t format() const;
    uint64_t modifier() const;

    std::shared_ptr<VulkanSwapchainSlot> acquire();
    void release(VulkanSwapchainSlot *slot, FileDescriptor &&releaseFd);

    static std::unique_ptr<VulkanSwapchain> create(VulkanDevice *device, GraphicsBufferAllocator *allocator, const QSize &size, uint32_t format, const ModifierList &modifiers);

private:
    VulkanDevice *const m_device;
    GraphicsBufferAllocator *const m_allocator;
    const QSize m_size;
    const uint32_t m_format;
    const uint64_t m_modifier;
    std::vector<std::shared_ptr<VulkanSwapchainSlot>> m_slots;
};

}

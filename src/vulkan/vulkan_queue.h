/*
    KWin - the KDE window manager
    This file is part of the KDE project.

    SPDX-FileCopyrightText: 2023-2026 Xaver Hugl <xaver.hugl@kde.org>

    SPDX-License-Identifier: GPL-2.0-or-later
*/
#pragma once
#include "core/graphicsbuffer.h"
#include "kwin_export.h"
#include "utils/filedescriptor.h"

#include <QSocketNotifier>
#include <deque>
#include <vulkan/vulkan_raii.hpp>

namespace KWin
{

class VulkanDevice;

class KWIN_EXPORT VulkanQueue
{
public:
    explicit VulkanQueue(VulkanDevice *device, uint32_t familyIndex, vk::raii::Queue &&handle, vk::raii::CommandPool &&commandPool);
    ~VulkanQueue();

    uint32_t familyIndex() const;
    const vk::raii::Queue &handle() const;

    vk::raii::CommandBuffer createCommandBuffer();
    std::optional<FileDescriptor> submit(vk::raii::CommandBuffer &&buffer, FileDescriptor &&syncFd, std::vector<GraphicsBufferRef> &&graphicsBuffers);

    /**
     * NOTE avoid using this if at all possible, it's obviously terrible for performance!
     */
    void waitIdle();

    static std::unique_ptr<VulkanQueue> create(VulkanDevice *device, uint32_t familyIndex);

private:
    struct SubmittedCommand
    {
        vk::raii::Semaphore waitSemaphore{nullptr};
        vk::raii::CommandBuffer buffer{nullptr};
        FileDescriptor completionSyncFd;
        QSocketNotifier notifier{QSocketNotifier::Read};
        std::vector<GraphicsBufferRef> graphicsBuffers;
    };

    VulkanDevice *const m_device;
    const uint32_t m_familyIndex;
    const vk::raii::Queue m_handle;
    const vk::raii::CommandPool m_commandPool;
    std::vector<std::unique_ptr<SubmittedCommand>> m_submittedCommandBuffers;
};

}

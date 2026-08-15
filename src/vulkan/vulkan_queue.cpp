/*
    KWin - the KDE window manager
    This file is part of the KDE project.

    SPDX-FileCopyrightText: 2023-2026 Xaver Hugl <xaver.hugl@kde.org>

    SPDX-License-Identifier: GPL-2.0-or-later
*/
#include "vulkan_queue.h"
#include "vulkan_device.h"
#include "vulkan_logging.h"

namespace KWin
{

VulkanQueue::VulkanQueue(VulkanDevice *device, uint32_t familyIndex, vk::raii::Queue &&handle, vk::raii::CommandPool &&commandPool)
    : m_device(device)
    , m_familyIndex(familyIndex)
    , m_handle(std::move(handle))
    , m_commandPool(std::move(commandPool))
{
}

VulkanQueue::~VulkanQueue()
{
    m_handle.waitIdle();
}

uint32_t VulkanQueue::familyIndex() const
{
    return m_familyIndex;
}

const vk::raii::Queue &VulkanQueue::handle() const
{
    return m_handle;
}

vk::raii::CommandBuffer VulkanQueue::createCommandBuffer()
{
    auto [result, buffers] = m_device->logicalDevice().allocateCommandBuffers(vk::CommandBufferAllocateInfo{
        m_commandPool,
        vk::CommandBufferLevel::ePrimary,
        1,
    });
    if (result != vk::Result::eSuccess) {
        qCWarning(KWIN_VULKAN) << "Failed to create a command buffer" << vk::to_string(result);
        return nullptr;
    }
    return std::move(buffers.front());
}

std::optional<FileDescriptor> VulkanQueue::submit(vk::raii::CommandBuffer &&buffer, FileDescriptor &&syncFd)
{
    vk::ExportFenceCreateInfo exportInfo{
        vk::ExternalFenceHandleTypeFlagBits::eSyncFd,
    };
    auto [fenceResult, fence] = m_device->logicalDevice().createFence(vk::FenceCreateInfo{
        vk::FenceCreateFlags{},
        &exportInfo,
    });
    if (fenceResult != vk::Result::eSuccess) {
        return std::nullopt;
    }
    std::vector<vk::Semaphore> waitSemaphores;
    std::vector<vk::PipelineStageFlags> waitFlags;
    auto waitSemaphore = m_device->importSemaphore(std::move(syncFd));
    if (waitSemaphore.has_value()) {
        waitSemaphores.push_back(*waitSemaphore);
        waitFlags.push_back(vk::PipelineStageFlagBits::eAllCommands);
    }
    vk::Result result = m_handle.submit(vk::SubmitInfo{
                                            waitSemaphores,
                                            waitFlags,
                                            *buffer,
                                            {},
                                        },
                                        fence);
    if (result == vk::Result::eErrorDeviceLost) {
        m_device->handleDeviceLoss();
        return std::nullopt;
    } else if (result != vk::Result::eSuccess) {
        return std::nullopt;
    }
    const auto [fdResult, fd] = m_device->logicalDevice().getFenceFdKHR(vk::FenceGetFdInfoKHR{
        fence,
        vk::ExternalFenceHandleTypeFlagBits::eSyncFd,
    });
    if (fdResult != vk::Result::eSuccess) {
        return std::nullopt;
    }
    FileDescriptor ret{fd};
    auto command = std::make_unique<SubmittedCommand>();
    if (waitSemaphore) {
        command->waitSemaphore = std::move(*waitSemaphore);
    }
    command->buffer = std::move(buffer),
    command->completionSyncFd = ret.duplicate(),
    command->notifier.setSocket(command->completionSyncFd.get());
    command->notifier.setEnabled(true);
    QObject::connect(&command->notifier, &QSocketNotifier::activated, &command->notifier, [this, cmd = command.get()]() {
        const auto it = std::ranges::find(m_submittedCommandBuffers, cmd, &std::unique_ptr<SubmittedCommand>::get);
        Q_ASSERT(it != m_submittedCommandBuffers.end());
        m_submittedCommandBuffers.erase(it);
    });
    m_submittedCommandBuffers.push_back(std::move(command));
    return ret;
}

void VulkanQueue::waitIdle()
{
    m_handle.waitIdle();
}

std::unique_ptr<VulkanQueue> VulkanQueue::create(VulkanDevice *device, uint32_t familyIndex)
{
    auto handle = device->logicalDevice().getQueue(familyIndex, 0);

    auto [result, cmdPool] = device->logicalDevice().createCommandPool(vk::CommandPoolCreateInfo{
        vk::CommandPoolCreateFlagBits::eResetCommandBuffer,
        familyIndex,
    });
    if (result != vk::Result::eSuccess) {
        qCCritical(KWIN_VULKAN) << "creating a command pool failed:" << vk::to_string(result);
        return nullptr;
    }
    return std::make_unique<VulkanQueue>(device, familyIndex, std::move(handle), std::move(cmdPool));
}

}

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

#include <QHash>
#include <QObject>
#include <QVector>
#include <deque>
#include <memory>
#include <optional>
#include <vulkan/vulkan.h>

namespace KWin
{

class VulkanTexture;
class GraphicsBuffer;
struct DmaBufAttributes;
class RenderDevice;

class KWIN_EXPORT VulkanDevice : public QObject
{
    Q_OBJECT

public:
    explicit VulkanDevice(VkInstance instance, VkPhysicalDevice physicalDevice, VkDevice logicalDevice, std::vector<VkQueueFamilyProperties> &&queueProperties);
    VulkanDevice(VulkanDevice &&other) = delete;
    VulkanDevice(const VulkanDevice &) = delete;
    ~VulkanDevice();

    std::shared_ptr<VulkanTexture> importBuffer(GraphicsBuffer *buffer, VkImageUsageFlags usage);
    std::shared_ptr<VulkanTexture> importDmabuf(const DmaBufAttributes *attributes, VkImageUsageFlags usage);

    const FormatModifierMap &supportedFormats() const;
    VkDevice logicalDevice() const;

    VkQueue transferQueue() const;
    VkCommandBuffer createCommandBuffer();
    VkSemaphore importSemaphore(FileDescriptor &&syncFd) const;

    std::optional<FileDescriptor> submit(VkCommandBuffer buffer, FileDescriptor &&syncFd);

    /**
     * Handle the "VK_ERROR_DEVICE_LOST" error by flagging this device as lost and releasing
     * all resources related to it. The render device will later delete this device and
     * (attempt to) create a new one.
     *
     * The error can happen with (swapchain and presentation related commands excluded):
     * - vkCreateDevice
     * - vkQueueSubmit
     * - vkGetFenceStatus
     * - vkWaitForFences
     * - vkGetSemaphoreCounterValue
     * - vkWaitSemaphoresKHR
     * - vkGetEventStatus
     * - vkQueueWaitIdle
     * - vkDeviceWaitIdle
     * - vkGetQueryPoolResults
     * - vkQueueBindSparse
     */
    void handleDeviceLoss();

Q_SIGNALS:
    /**
     * This signal is emitted when the associated Vulkan device has been
     * lost, and before it is deleted. In response, all Vulkan resources
     * of this device must be released.
     * If creating a new Vulkan device is successful, resources can be
     * re-created at a later time.
     */
    void deviceLost();

private:
    FormatModifierMap queryFormats(VkImageUsageFlags flags) const;
    void createQueues();
    std::optional<uint32_t> findMemoryType(uint32_t typeBits, VkMemoryPropertyFlags memoryPropertyFlags) const;

    VkPhysicalDevice m_physical = nullptr;
    VkDevice m_logical = nullptr;
    FormatModifierMap m_formats;
    std::vector<VkQueueFamilyProperties> m_queueProperties;
    VkQueue m_transferQueue = nullptr;
    VkCommandPool m_commandPool = nullptr;
    uint32_t m_queueFamilyIndex;
    VkPhysicalDeviceMemoryProperties m_memoryProperties;
    struct SubmittedCommand
    {
        VkSemaphore waitSemaphore;
        VkCommandBuffer buffer;
        FileDescriptor completionSyncFd;
    };
    std::deque<SubmittedCommand> m_submittedCommandBuffers;

    QHash<GraphicsBuffer *, std::shared_ptr<VulkanTexture>> m_importedTextures;
    bool m_lost = false;
};

}

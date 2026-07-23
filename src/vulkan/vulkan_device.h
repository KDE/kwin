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
#include <vulkan/vulkan_raii.hpp>

namespace KWin
{

class VulkanTexture;
class GraphicsBuffer;
struct DmaBufAttributes;
class RenderDevice;
class VulkanDevice;

class KWIN_EXPORT VulkanQueue
{
public:
    explicit VulkanQueue(VulkanDevice *device, uint32_t familyIndex, vk::raii::Queue &&handle, vk::raii::CommandPool &&commandPool);
    ~VulkanQueue();

    uint32_t familyIndex() const;
    const vk::raii::Queue &handle() const;

    vk::raii::CommandBuffer createCommandBuffer();
    std::optional<FileDescriptor> submit(vk::raii::CommandBuffer &&buffer, FileDescriptor &&syncFd);

    /**
     * NOTE avoid using this if at all possible, it's obviously terrible for performance!
     */
    void waitIdle();

    static std::unique_ptr<VulkanQueue> create(VulkanDevice *device, uint32_t familyIndex);

private:
    struct SubmittedCommand
    {
        vk::raii::Semaphore waitSemaphore;
        vk::raii::CommandBuffer buffer;
        FileDescriptor completionSyncFd;
    };

    VulkanDevice *const m_device;
    const uint32_t m_familyIndex;
    const vk::raii::Queue m_handle;
    const vk::raii::CommandPool m_commandPool;
    std::deque<SubmittedCommand> m_submittedCommandBuffers;
};

class KWIN_EXPORT VulkanDevice : public QObject
{
    Q_OBJECT

public:
    explicit VulkanDevice(vk::raii::PhysicalDevice physicalDevice, vk::raii::Device &&logicalDevice,
                          std::vector<VkQueueFamilyProperties> &&queueProperties, vk::PhysicalDeviceType type);
    VulkanDevice(VulkanDevice &&other) = delete;
    VulkanDevice(const VulkanDevice &) = delete;
    ~VulkanDevice();

    std::shared_ptr<VulkanTexture> importBuffer(GraphicsBuffer *buffer, VkImageUsageFlags usage);

    bool isSoftwareRenderer() const;
    vk::PhysicalDeviceType type() const;

    vk::raii::DeviceMemory allocateMemory(const vk::ImageCreateInfo &imageInfo, vk::MemoryPropertyFlags memoryProperties);
    vk::raii::DeviceMemory allocateMemory(const vk::BufferCreateInfo &bufferInfo, vk::MemoryPropertyFlags memoryProperties);

    const FormatModifierMap &supportedFormats() const;
    const vk::raii::Device &logicalDevice() const;

    VulkanQueue *graphicsQueue() const;
    VulkanQueue *transferQueue() const;

    std::span<const VkQueueFamilyProperties> queueFamilyProperties() const;
    float nanosecondsPerQueryTick() const;

    std::optional<vk::raii::Semaphore> importSemaphore(FileDescriptor &&syncFd) const;

    /**
     * NOTE avoid using this if at all possible, it's obviously terrible for performance!
     */
    void waitIdle();

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
    void getQueues();
    FormatModifierMap queryFormats(VkImageUsageFlags flags) const;
    std::optional<uint32_t> findMemoryType(uint32_t typeBits, vk::MemoryPropertyFlags memoryPropertyFlags) const;
    std::shared_ptr<VulkanTexture> importDmabuf(const DmaBufAttributes *attributes, VkImageUsageFlags usage);

    vk::PhysicalDeviceType m_type;
    vk::raii::PhysicalDevice m_physical;
    vk::raii::Device m_logical;
    FormatModifierMap m_formats;
    std::vector<VkQueueFamilyProperties> m_queueProperties;
    vk::PhysicalDeviceMemoryProperties m_memoryProperties;
    vk::PhysicalDeviceLimits m_deviceLimits;

    std::unique_ptr<VulkanQueue> m_graphicsQueue;
    std::unique_ptr<VulkanQueue> m_transferQueue;

    QHash<GraphicsBuffer *, std::shared_ptr<VulkanTexture>> m_importedTextures;
    bool m_lost = false;
};

}

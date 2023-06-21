#pragma once

#include <QSize>
#include <memory>
#include <vector>

#include "kwin_export.h"
#include "vulkan_include.h"

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
    explicit VulkanSwapchainSlot(GraphicsBuffer *buffer, std::unique_ptr<VulkanTexture> &&texture, vk::UniqueCommandBuffer &&commandBuffer);
    ~VulkanSwapchainSlot();

    GraphicsBuffer *buffer() const;
    VulkanTexture *texture() const;
    vk::CommandBuffer commandBuffer() const;
    int age() const;

private:
    GraphicsBuffer *const m_buffer;
    std::unique_ptr<VulkanTexture> m_texture;
    vk::UniqueCommandBuffer m_commandBuffer;
    int m_age = 0;
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
    void release(VulkanSwapchainSlot *slot);

    static std::shared_ptr<VulkanSwapchain> create(VulkanDevice *device, GraphicsBufferAllocator *allocator, const QSize &size, uint32_t format, const QVector<uint64_t> &modifiers);

private:
    VulkanDevice *const m_device;
    GraphicsBufferAllocator *const m_allocator;
    const QSize m_size;
    const uint32_t m_format;
    const uint64_t m_modifier;
    std::vector<std::shared_ptr<VulkanSwapchainSlot>> m_slots;
};

}

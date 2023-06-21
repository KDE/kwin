#include "vulkan_swapchain.h"
#include "core/graphicsbuffer.h"
#include "core/graphicsbufferallocator.h"
#include "vulkan_device.h"
#include "vulkan_texture.h"

namespace KWin
{

VulkanSwapchainSlot::VulkanSwapchainSlot(GraphicsBuffer *buffer, std::unique_ptr<VulkanTexture> &&texture, vk::UniqueCommandBuffer &&commandBuffer)
    : m_buffer(buffer)
    , m_texture(std::move(texture))
    , m_commandBuffer(std::move(commandBuffer))
{
}

VulkanSwapchainSlot::~VulkanSwapchainSlot()
{
    m_buffer->drop();
}

GraphicsBuffer *VulkanSwapchainSlot::buffer() const
{
    return m_buffer;
}

VulkanTexture *VulkanSwapchainSlot::texture() const
{
    return m_texture.get();
}

vk::CommandBuffer VulkanSwapchainSlot::commandBuffer() const
{
    return m_commandBuffer.get();
}

int VulkanSwapchainSlot::age() const
{
    return m_age;
}

VulkanSwapchain::VulkanSwapchain(VulkanDevice *device, GraphicsBufferAllocator *allocator, const QSize &size, uint32_t format, uint64_t modifier, const std::shared_ptr<VulkanSwapchainSlot> &initialSlot)
    : m_device(device)
    , m_allocator(allocator)
    , m_size(size)
    , m_format(format)
    , m_modifier(modifier)
    , m_slots({initialSlot})
{
}

VulkanSwapchain::~VulkanSwapchain() = default;

QSize VulkanSwapchain::size() const
{
    return m_size;
}

uint32_t VulkanSwapchain::format() const
{
    return m_format;
}

uint64_t VulkanSwapchain::modifier() const
{
    return m_modifier;
}

std::shared_ptr<VulkanSwapchainSlot> VulkanSwapchain::acquire()
{
    for (const auto &slot : m_slots) {
        if (!slot->buffer()->isReferenced()) {
            return slot;
        }
    }

    GraphicsBuffer *buffer = m_allocator->allocate(GraphicsBufferOptions{
        .size = m_size,
        .format = m_format,
        .modifiers = {m_modifier},
    });
    if (!buffer) {
        qCWarning(KWIN_VULKAN) << "Failed to allocate a vulkan swapchain buffer";
        return nullptr;
    }

    auto texture = m_device->importDmabuf(buffer);
    if (!texture) {
        return nullptr;
    }
    auto slot = std::make_shared<VulkanSwapchainSlot>(buffer, std::make_unique<VulkanTexture>(std::move(*texture)), m_device->allocateCommandBuffer());
    m_slots.push_back(slot);
    return slot;
}

void VulkanSwapchain::release(VulkanSwapchainSlot *released)
{
    for (const auto &slot : m_slots) {
        if (slot.get() == released) {
            slot->m_age = 1;
        } else if (slot->age() > 0) {
            slot->m_age++;
        }
    }
}

std::shared_ptr<VulkanSwapchain> VulkanSwapchain::create(VulkanDevice *device, GraphicsBufferAllocator *allocator, const QSize &size, uint32_t format, const QVector<uint64_t> &modifiers)
{
    GraphicsBuffer *buffer = allocator->allocate(GraphicsBufferOptions{
        .size = size,
        .format = format,
        .modifiers = modifiers,
    });
    if (!buffer) {
        qCWarning(KWIN_VULKAN) << "Failed to allocate a graphics buffer for a Vulkan swapchain";
        return nullptr;
    }
    auto texture = device->importDmabuf(buffer);
    if (!texture) {
        return nullptr;
    }
    auto slot = std::make_shared<VulkanSwapchainSlot>(buffer, std::make_unique<VulkanTexture>(std::move(*texture)), device->allocateCommandBuffer());
    return std::make_shared<VulkanSwapchain>(device, allocator, size, format, buffer->dmabufAttributes()->modifier, slot);
}

}

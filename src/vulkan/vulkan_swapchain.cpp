/*
    KWin - the KDE window manager
    This file is part of the KDE project.

    SPDX-FileCopyrightText: 2023-2026 Xaver Hugl <xaver.hugl@kde.org>

    SPDX-License-Identifier: GPL-2.0-or-later
*/
#include "vulkan_swapchain.h"
#include "core/graphicsbuffer.h"
#include "core/graphicsbufferallocator.h"
#include "vulkan_device.h"
#include "vulkan_logging.h"
#include "vulkan_texture.h"

namespace KWin
{

VulkanSwapchainSlot::VulkanSwapchainSlot(GraphicsBuffer *buffer, std::unique_ptr<VulkanTexture> &&texture)
    : m_buffer(buffer)
    , m_texture(std::move(texture))
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

int VulkanSwapchainSlot::age() const
{
    return m_age;
}

bool VulkanSwapchainSlot::isBusy() const
{
    return m_buffer->isReferenced() || (m_releaseFd.isValid() && !m_releaseFd.isReadable());
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

VulkanSwapchain::~VulkanSwapchain()
{
}

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
        if (!slot->isBusy()) {
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

    auto texture = m_device->importDmabuf(buffer->dmabufAttributes(), VK_IMAGE_USAGE_SAMPLED_BIT | VK_IMAGE_USAGE_TRANSFER_DST_BIT);
    if (!texture) {
        return nullptr;
    }
    auto slot = std::make_shared<VulkanSwapchainSlot>(buffer, std::make_unique<VulkanTexture>(std::move(*texture)));
    m_slots.push_back(slot);
    return slot;
}

void VulkanSwapchain::release(VulkanSwapchainSlot *released, FileDescriptor &&releaseFd)
{
    released->m_releaseFd = std::move(releaseFd);
    for (const auto &slot : m_slots) {
        if (slot.get() == released) {
            slot->m_age = 1;
        } else if (slot->age() > 0) {
            slot->m_age++;
        }
    }
}

std::unique_ptr<VulkanSwapchain> VulkanSwapchain::create(VulkanDevice *device, GraphicsBufferAllocator *allocator, const QSize &size, uint32_t format, const ModifierList &modifiers)
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
    auto texture = device->importDmabuf(buffer->dmabufAttributes(), VK_IMAGE_USAGE_SAMPLED_BIT | VK_IMAGE_USAGE_TRANSFER_DST_BIT);
    if (!texture) {
        return nullptr;
    }
    auto slot = std::make_shared<VulkanSwapchainSlot>(buffer, std::make_unique<VulkanTexture>(std::move(*texture)));
    return std::make_unique<VulkanSwapchain>(device, allocator, size, format, buffer->dmabufAttributes()->modifier, slot);
}

}

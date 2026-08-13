/*
    KWin - the KDE window manager
    This file is part of the KDE project.

    SPDX-FileCopyrightText: 2023-2026 Xaver Hugl <xaver.hugl@kde.org>

    SPDX-License-Identifier: GPL-2.0-or-later
*/
#include "vulkan_buffer_swapchain.h"
#include "core/graphicsbuffer.h"
#include "core/hostmemoryallocator.h"
#include "core/syncobjtimeline.h"
#include "vulkan_device.h"
#include "vulkan_logging.h"
#include "vulkan_texture.h"

namespace KWin
{

VulkanBufferSwapchainSlot::VulkanBufferSwapchainSlot(GraphicsBuffer *buffer, std::shared_ptr<VulkanBuffer> &&vulkanBuffer)
    : m_buffer(buffer)
    , m_vulkanBuffer(std::move(vulkanBuffer))
    , m_releasePoint(std::make_shared<GraphicsBufferReleasePoint>())
{
}

VulkanBufferSwapchainSlot::~VulkanBufferSwapchainSlot()
{
    m_releasePoint->setBuffer(m_buffer);
    m_buffer->drop();
}

GraphicsBuffer *VulkanBufferSwapchainSlot::buffer() const
{
    return m_buffer;
}

VulkanBuffer *VulkanBufferSwapchainSlot::vulkanBuffer() const
{
    return m_vulkanBuffer.get();
}

int VulkanBufferSwapchainSlot::age() const
{
    return m_age;
}

bool VulkanBufferSwapchainSlot::isBusy() const
{
    return m_buffer->isReferenced()
        || m_releasePoint.use_count() > 1
        || (m_releasePoint->releaseFd().isValid() && !m_releasePoint->releaseFd().isReadable());
}

const FileDescriptor &VulkanBufferSwapchainSlot::releaseFd() const
{
    return m_releasePoint->releaseFd();
}

std::shared_ptr<SyncReleasePoint> VulkanBufferSwapchainSlot::releasePoint()
{
    return m_releasePoint;
}

VulkanBufferSwapchain::VulkanBufferSwapchain(VulkanDevice *device, const std::shared_ptr<HostMemoryGraphicsBufferAllocator> &allocator, const GraphicsBufferOptions &options, std::shared_ptr<VulkanBufferSwapchainSlot> &&initialSlot)
    : m_device(device)
    , m_allocator(allocator)
    , m_options(options)
    , m_slots({std::move(initialSlot)})
{
}

VulkanBufferSwapchain::~VulkanBufferSwapchain()
{
}

QSize VulkanBufferSwapchain::size() const
{
    return m_options.size;
}

uint32_t VulkanBufferSwapchain::format() const
{
    return m_options.format;
}

uint64_t VulkanBufferSwapchain::modifier() const
{
    return m_options.modifiers.front();
}

bool VulkanBufferSwapchain::scanout() const
{
    return m_options.scanout;
}

std::shared_ptr<VulkanBufferSwapchainSlot> VulkanBufferSwapchain::acquire()
{
    for (const auto &slot : m_slots) {
        if (!slot->isBusy()) {
            return slot;
        }
    }

    GraphicsBuffer *buffer = m_allocator->allocate(m_options);
    if (!buffer) {
        qCWarning(KWIN_VULKAN) << "Failed to allocate a vulkan swapchain buffer";
        return nullptr;
    }

    auto vulkanBuffer = m_device->importBufferAsBuffer(buffer, vk::BufferUsageFlagBits::eTransferDst);
    if (!vulkanBuffer) {
        buffer->drop();
        return nullptr;
    }
    auto slot = std::make_shared<VulkanBufferSwapchainSlot>(buffer, std::move(vulkanBuffer));
    m_slots.push_back(slot);
    return slot;
}

void VulkanBufferSwapchain::release(VulkanBufferSwapchainSlot *released, FileDescriptor &&releaseFd)
{
    released->m_releasePoint->addReleaseFence(releaseFd);
    for (const auto &slot : m_slots) {
        if (slot.get() == released) {
            slot->m_age = 1;
        } else if (slot->age() > 0) {
            slot->m_age++;
        }
    }
}

void VulkanBufferSwapchain::resetBufferAge()
{
    for (const auto &slot : std::as_const(m_slots)) {
        slot->m_age = 0;
    }
}

std::unique_ptr<VulkanBufferSwapchain> VulkanBufferSwapchain::create(VulkanDevice *device, const std::shared_ptr<HostMemoryGraphicsBufferAllocator> &allocator, const GraphicsBufferOptions &options)
{
    GraphicsBuffer *buffer = allocator->allocate(options);
    if (!buffer) {
        qCWarning(KWIN_VULKAN) << "Failed to allocate a graphics buffer for a Vulkan buffer swapchain";
        return nullptr;
    }
    auto vulkanBuffer = device->importBufferAsBuffer(buffer, vk::BufferUsageFlagBits::eTransferDst);
    if (!vulkanBuffer) {
        buffer->drop();
        return nullptr;
    }
    auto slot = std::make_shared<VulkanBufferSwapchainSlot>(buffer, std::move(vulkanBuffer));
    return std::make_unique<VulkanBufferSwapchain>(device, allocator, options, std::move(slot));
}

}

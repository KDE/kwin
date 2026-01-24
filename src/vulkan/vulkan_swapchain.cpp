/*
    KWin - the KDE window manager
    This file is part of the KDE project.

    SPDX-FileCopyrightText: 2023-2026 Xaver Hugl <xaver.hugl@kde.org>

    SPDX-License-Identifier: GPL-2.0-or-later
*/
#include "vulkan_swapchain.h"
#include "core/graphicsbuffer.h"
#include "core/graphicsbufferallocator.h"
#include "core/syncobjtimeline.h"
#include "vulkan_device.h"
#include "vulkan_logging.h"
#include "vulkan_texture.h"

namespace KWin
{

class VulkanReleasePoint : public SyncReleasePoint
{
public:
    explicit VulkanReleasePoint(const std::shared_ptr<VulkanSwapchainSlot> &slot);

    void addReleaseFence(const FileDescriptor &fd) override;

private:
    std::shared_ptr<VulkanSwapchainSlot> m_slot;
};

VulkanReleasePoint::VulkanReleasePoint(const std::shared_ptr<VulkanSwapchainSlot> &slot)
    : m_slot(slot)
{
}

void VulkanReleasePoint::addReleaseFence(const FileDescriptor &fd)
{
    if (m_slot->m_releaseFd.isValid()) {
        m_slot->m_releaseFd = m_slot->m_releaseFd.mergeSyncFds(fd, m_slot->m_releaseFd);
    } else {
        m_slot->m_releaseFd = fd.duplicate();
    }
}

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
    return m_buffer->isReferenced() || !m_releasePoint.expired() || (m_releaseFd.isValid() && !m_releaseFd.isReadable());
}

const FileDescriptor &VulkanSwapchainSlot::releaseFd() const
{
    return m_releaseFd;
}

std::shared_ptr<SyncReleasePoint> VulkanSwapchainSlot::releasePoint()
{
    auto ret = m_releasePoint.lock();
    if (!ret) {
        ret = std::make_shared<VulkanReleasePoint>(shared_from_this());
        m_releasePoint = ret;
    }
    return ret;
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

void VulkanSwapchain::resetBufferAge()
{
    for (const auto &slot : std::as_const(m_slots)) {
        slot->m_age = 0;
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

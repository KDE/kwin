/*
    KWin - the KDE window manager
    This file is part of the KDE project.

    SPDX-FileCopyrightText: 2023-2026 Xaver Hugl <xaver.hugl@kde.org>

    SPDX-License-Identifier: GPL-2.0-or-later
*/
#include "generic_swapchain.h"
#include "core/graphicsbuffer.h"
#include "core/graphicsbufferallocator.h"
#include "core/syncobjtimeline.h"
#include "utils/common.h"

namespace KWin
{

SwapchainSlot::SwapchainSlot(GraphicsBuffer *buffer)
    : m_buffer(buffer)
    , m_releasePoint(std::make_shared<GraphicsBufferReleasePoint>())
{
}

SwapchainSlot::~SwapchainSlot()
{
    m_releasePoint->setBuffer(m_buffer);
    m_buffer->drop();
}

GraphicsBuffer *SwapchainSlot::buffer() const
{
    return m_buffer;
}

int SwapchainSlot::age() const
{
    return m_age;
}

bool SwapchainSlot::isBusy() const
{
    return m_buffer->isReferenced()
        || m_releasePoint.use_count() > 1
        || (m_releasePoint->releaseFd().isValid() && !m_releasePoint->releaseFd().isReadable());
}

const FileDescriptor &SwapchainSlot::releaseFd() const
{
    return m_releasePoint->releaseFd();
}

std::shared_ptr<SyncReleasePoint> SwapchainSlot::releasePoint()
{
    return m_releasePoint;
}

Swapchain::Swapchain(GraphicsBufferAllocator *allocator, const GraphicsBufferOptions &options, std::shared_ptr<SwapchainSlot> &&initialSlot)
    : m_allocator(allocator)
    , m_options(options)
    , m_slots({std::move(initialSlot)})
{
}

Swapchain::~Swapchain()
{
}

QSize Swapchain::size() const
{
    return m_options.size;
}

uint32_t Swapchain::format() const
{
    return m_options.format;
}

uint64_t Swapchain::modifier() const
{
    return m_options.modifiers.front();
}

bool Swapchain::scanout() const
{
    return m_options.scanout;
}

std::shared_ptr<SwapchainSlot> Swapchain::acquire()
{
    for (const auto &slot : m_slots) {
        if (!slot->isBusy()) {
            return slot;
        }
    }

    GraphicsBuffer *buffer = m_allocator->allocate(m_options);
    if (!buffer) {
        qCWarning(KWIN_CORE) << "Failed to allocate a swapchain buffer";
        return nullptr;
    }

    auto slot = std::make_shared<SwapchainSlot>(buffer);
    m_slots.push_back(slot);
    return slot;
}

void Swapchain::release(SwapchainSlot *released, FileDescriptor &&releaseFd)
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

void Swapchain::resetBufferAge()
{
    for (const auto &slot : std::as_const(m_slots)) {
        slot->m_age = 0;
    }
}

std::unique_ptr<Swapchain> Swapchain::create(GraphicsBufferAllocator *allocator, GraphicsBufferOptions options)
{
    GraphicsBuffer *buffer = allocator->allocate(options);
    if (!buffer) {
        qCWarning(KWIN_CORE) << "Failed to allocate a graphics buffer for a swapchain";
        return nullptr;
    }
    options.modifiers = {buffer->dmabufAttributes()->modifier};
    auto slot = std::make_shared<SwapchainSlot>(buffer);
    return std::make_unique<Swapchain>(allocator, options, std::move(slot));
}

}

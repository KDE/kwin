/*
    KWin - the KDE window manager
    This file is part of the KDE project.

    SPDX-FileCopyrightText: 2026 Xaver Hugl <xaver.hugl@kde.org>

    SPDX-License-Identifier: GPL-2.0-or-later
*/
#include "vulkan_buffer.h"

namespace KWin
{

VulkanShmBuffer::VulkanShmBuffer(GraphicsBuffer *buffer, vk::raii::Buffer &&handle, vk::raii::DeviceMemory &&memory)
    : m_buffer(buffer)
    , m_handle(std::move(handle))
    , m_memory(std::move(memory))
{
}

VulkanShmBuffer::~VulkanShmBuffer()
{
    m_buffer->unmap();
}

GraphicsBuffer *VulkanShmBuffer::buffer() const
{
    return m_buffer.buffer();
}

vk::Buffer VulkanShmBuffer::handle() const
{
    return m_handle;
}

vk::DeviceMemory VulkanShmBuffer::memory() const
{
    return m_memory;
}

}

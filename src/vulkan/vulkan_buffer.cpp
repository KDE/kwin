/*
    KWin - the KDE window manager
    This file is part of the KDE project.

    SPDX-FileCopyrightText: 2026 Xaver Hugl <xaver.hugl@kde.org>

    SPDX-License-Identifier: GPL-2.0-or-later
*/
#include "vulkan_buffer.h"

namespace KWin
{

VulkanBuffer::VulkanBuffer(vk::raii::Buffer &&handle, vk::raii::DeviceMemory &&memory, vk::DeviceSize size)
    : m_handle(std::move(handle))
    , m_memory(std::move(memory))
    , m_size(size)
{
}

const vk::raii::Buffer &VulkanBuffer::handle() const
{
    return m_handle;
}

const vk::raii::DeviceMemory &VulkanBuffer::memory() const
{
    return m_memory;
}

vk::DeviceSize VulkanBuffer::size() const
{
    return m_size;
}

}

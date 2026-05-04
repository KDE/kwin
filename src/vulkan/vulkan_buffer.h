/*
    KWin - the KDE window manager
    This file is part of the KDE project.

    SPDX-FileCopyrightText: 2026 Xaver Hugl <xaver.hugl@kde.org>

    SPDX-License-Identifier: GPL-2.0-or-later
*/
#pragma once
#include "core/graphicsbuffer.h"

#include <vulkan/vulkan_raii.hpp>

namespace KWin
{

class VulkanShmBuffer
{
public:
    explicit VulkanShmBuffer(GraphicsBuffer *buffer, vk::raii::Buffer &&handle, vk::raii::DeviceMemory &&memory);
    ~VulkanShmBuffer();

    GraphicsBuffer *buffer() const;
    vk::Buffer handle() const;
    vk::DeviceMemory memory() const;

private:
    GraphicsBufferRef m_buffer;
    vk::raii::Buffer m_handle;
    vk::raii::DeviceMemory m_memory;
};

}

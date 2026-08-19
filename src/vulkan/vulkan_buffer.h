/*
    KWin - the KDE window manager
    This file is part of the KDE project.

    SPDX-FileCopyrightText: 2026 Xaver Hugl <xaver.hugl@kde.org>

    SPDX-License-Identifier: GPL-2.0-or-later
*/
#pragma once
#include "kwin_export.h"

#include <vulkan/vulkan_raii.hpp>

namespace KWin
{

class KWIN_EXPORT VulkanBuffer
{
public:
    explicit VulkanBuffer(vk::raii::Buffer &&handle, vk::raii::DeviceMemory &&memory, vk::DeviceSize size);

    const vk::raii::Buffer &handle() const;
    const vk::raii::DeviceMemory &memory() const;
    vk::DeviceSize size() const;

private:
    vk::raii::Buffer m_handle;
    vk::raii::DeviceMemory m_memory;
    vk::DeviceSize m_size;
};

}

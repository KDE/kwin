/*
    KWin - the KDE window manager
    This file is part of the KDE project.

    SPDX-FileCopyrightText: 2023-2026 Xaver Hugl <xaver.hugl@kde.org>

    SPDX-License-Identifier: GPL-2.0-or-later
*/
#include "vulkan_texture.h"
#include "vulkan_device.h"

#include <utility>

namespace KWin
{

VulkanTexture::VulkanTexture(VulkanDevice *device, VkFormat format, vk::raii::Image &&image, std::vector<vk::raii::DeviceMemory> &&memory)
    : m_device(device)
    , m_format(format)
    , m_memory(std::move(memory))
    , m_image(std::move(image))
{
}

VulkanTexture::~VulkanTexture()
{
}

const vk::raii::Image &VulkanTexture::handle() const
{
    return m_image;
}

VkFormat VulkanTexture::format() const
{
    return m_format;
}

}

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

VulkanTexture::VulkanTexture(VulkanDevice *device, VkFormat format, VkImage image, std::vector<VkDeviceMemory> &&memory)
    : m_device(device)
    , m_format(format)
    , m_memory(std::move(memory))
    , m_image(image)
{
}

VulkanTexture::~VulkanTexture()
{
    vkDestroyImage(m_device->logicalDevice(), m_image, nullptr);
    for (const VkDeviceMemory &memory : m_memory) {
        vkFreeMemory(m_device->logicalDevice(), memory, nullptr);
    }
}

VkImage VulkanTexture::handle() const
{
    return m_image;
}

VkFormat VulkanTexture::format() const
{
    return m_format;
}

}

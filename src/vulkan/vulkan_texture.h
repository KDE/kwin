/*
    KWin - the KDE window manager
    This file is part of the KDE project.

    SPDX-FileCopyrightText: 2023-2026 Xaver Hugl <xaver.hugl@kde.org>

    SPDX-License-Identifier: GPL-2.0-or-later
*/
#pragma once
#include "kwin_export.h"

#include <vulkan/vulkan_raii.hpp>

namespace KWin
{

class VulkanDevice;

class KWIN_EXPORT VulkanTexture
{
public:
    explicit VulkanTexture(VulkanDevice *device, VkFormat format, vk::raii::Image &&image, std::vector<vk::raii::DeviceMemory> &&memory);
    VulkanTexture(VulkanTexture &&other) = delete;
    VulkanTexture(const VulkanTexture &) = delete;
    ~VulkanTexture();

    const vk::raii::Image &handle() const;
    VkFormat format() const;

private:
    VulkanDevice *m_device;
    VkFormat m_format;
    std::vector<vk::raii::DeviceMemory> m_memory;
    vk::raii::Image m_image;
};

}

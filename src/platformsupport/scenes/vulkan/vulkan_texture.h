/*
    KWin - the KDE window manager
    This file is part of the KDE project.

    SPDX-FileCopyrightText: 2023 Xaver Hugl <xaver.hugl@gmail.com>

    SPDX-License-Identifier: GPL-2.0-or-later
*/
#pragma once
#include "vulkan_include.h"

namespace KWin
{

class VulkanTexture
{
public:
    VulkanTexture(vk::Format format, vk::UniqueImage &&image, std::vector<vk::UniqueDeviceMemory> &&memory, vk::UniqueImageView &&imageView);
    VulkanTexture(const VulkanTexture &) = delete;
    VulkanTexture(VulkanTexture &&other);
    ~VulkanTexture();

private:
    vk::Format m_format;
    std::vector<vk::UniqueDeviceMemory> m_memory;
    vk::UniqueImage m_image;
    vk::UniqueImageView m_view;
};

}

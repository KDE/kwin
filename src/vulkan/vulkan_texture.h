/*
    KWin - the KDE window manager
    This file is part of the KDE project.

    SPDX-FileCopyrightText: 2023-2026 Xaver Hugl <xaver.hugl@kde.org>

    SPDX-License-Identifier: GPL-2.0-or-later
*/
#pragma once
#include "kwin_export.h"

#include <vulkan/vulkan.h>

#include <QImage>

namespace KWin
{

class VulkanDevice;

class KWIN_EXPORT VulkanTexture
{
public:
    explicit VulkanTexture(VulkanDevice *device, VkFormat format, VkImage image, std::vector<VkDeviceMemory> &&memory);
    explicit VulkanTexture(VulkanTexture &&other);
    VulkanTexture(const VulkanTexture &) = delete;
    ~VulkanTexture();

    VkImage handle() const;
    VkFormat format() const;

private:
    VulkanDevice *m_device;
    VkFormat m_format;
    std::vector<VkDeviceMemory> m_memory;
    VkImage m_image;
};

}

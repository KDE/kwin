/*
    KWin - the KDE window manager
    This file is part of the KDE project.

    SPDX-FileCopyrightText: 2023 Xaver Hugl <xaver.hugl@gmail.com>

    SPDX-License-Identifier: GPL-2.0-or-later
*/
#pragma once
#include "kwin_export.h"
#include "vulkan_include.h"

#include <QImage>

namespace KWin
{

class VulkanDevice;

class KWIN_EXPORT VulkanTexture
{
public:
    explicit VulkanTexture(vk::Format format, vk::UniqueImage &&image, std::vector<vk::UniqueDeviceMemory> &&memory, vk::UniqueImageView &&imageView);
    explicit VulkanTexture(VulkanTexture &&other);
    VulkanTexture(const VulkanTexture &) = delete;
    ~VulkanTexture();

    vk::Image handle() const;
    vk::ImageView view() const;
    vk::Format format() const;

    static std::unique_ptr<VulkanTexture> allocate(VulkanDevice *device, vk::Format format, const QSize &size);
    static std::unique_ptr<VulkanTexture> upload(VulkanDevice *device, const QImage &image);

private:
    vk::Format m_format;
    std::vector<vk::UniqueDeviceMemory> m_memory;
    vk::UniqueImage m_image;
    vk::UniqueImageView m_view;
    vk::ImageLayout m_layout = vk::ImageLayout::eUndefined;
};

}

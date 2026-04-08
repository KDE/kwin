/*
    KWin - the KDE window manager
    This file is part of the KDE project.

    SPDX-FileCopyrightText: 2023-2026 Xaver Hugl <xaver.hugl@kde.org>

    SPDX-License-Identifier: GPL-2.0-or-later
*/
#pragma once
#include "core/region.h"
#include "kwin_export.h"

#include <QImage>
#include <QSize>
#include <vulkan/vulkan_raii.hpp>

namespace KWin
{

class VulkanDevice;

class KWIN_EXPORT VulkanTexture
{
public:
    struct FormatWithSwizzle
    {
        vk::Format format;
        vk::ComponentMapping swizzles;
        std::optional<QImage::Format> intermediaryCopy;
    };
    static std::optional<FormatWithSwizzle> qImageToVulkanFormat(QImage::Format format);
    static std::unique_ptr<VulkanTexture> allocate(VulkanDevice *device, vk::Format format, vk::ComponentMapping swizzles, const QSize &size, vk::ImageUsageFlags usage);
    static std::unique_ptr<VulkanTexture> upload(VulkanDevice *device, const QImage &image, vk::ImageUsageFlags usage);

    explicit VulkanTexture(VulkanDevice *device, vk::Format format, vk::ComponentMapping viewSwizzles,
                           vk::raii::Image &&image, std::vector<vk::raii::DeviceMemory> &&memory,
                           const QSize &size, vk::raii::ImageView &&view);
    VulkanTexture(VulkanTexture &&other) = delete;
    VulkanTexture(const VulkanTexture &) = delete;
    ~VulkanTexture();

    /**
     * NOTE the format and size have to match in order for the update to work
     */
    bool update(const QImage &img, const Region &region, const QPoint &offset = QPoint());
    /**
     * NOTE the format and size have to match in order for the update to work
     */
    bool update(const QImage &img);

    QImage download() const;

    const vk::raii::Image &handle() const;
    const vk::raii::ImageView &view() const;
    const vk::raii::Sampler &sampler();
    vk::Format format() const;
    vk::ComponentMapping swizzles() const;
    QSize size() const;

private:
    VulkanDevice *m_device;
    vk::Format m_format;
    vk::ComponentMapping m_swizzles;
    std::vector<vk::raii::DeviceMemory> m_memory;
    vk::raii::Image m_image;
    QSize m_size;
    vk::raii::ImageView m_view;
    vk::raii::Sampler m_sampler;
};

}

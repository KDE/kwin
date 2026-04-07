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
class VulkanDescriptor;

class KWIN_EXPORT VulkanTexture
{
public:
    static std::optional<vk::Format> qImageToVulkanFormat(QImage::Format format);
    static std::unique_ptr<VulkanTexture> allocate(VulkanDevice *device, vk::Format format, const QSize &size, vk::ImageUsageFlags usage);
    static std::unique_ptr<VulkanTexture> upload(VulkanDevice *device, const QImage &image, vk::ImageUsageFlags usage);
    static vk::raii::Sampler createSampler(VulkanDevice *device);

    explicit VulkanTexture(VulkanDevice *device, vk::Format format, vk::raii::Image &&image,
                           vk::raii::ImageView &&view, vk::raii::Sampler &&sampler,
                           std::vector<vk::raii::DeviceMemory> &&memory, const QSize &size);
    VulkanTexture(VulkanTexture &&other) = delete;
    VulkanTexture(const VulkanTexture &) = delete;
    ~VulkanTexture();

    VulkanDevice *device() const;

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
    const vk::raii::Sampler &sampler() const;
    vk::Format format() const;
    QSize size() const;

private:
    VulkanDevice *m_device;
    vk::Format m_format;
    std::vector<vk::raii::DeviceMemory> m_memory;
    vk::raii::Image m_image;
    vk::raii::ImageView m_view;
    vk::raii::Sampler m_sampler;
    QSize m_size;
};

}

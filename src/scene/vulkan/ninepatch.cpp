/*
    SPDX-FileCopyrightText: 2026 Vlad Zahorodnii <vlad.zahorodnii@kde.org>
    SPDX-FileCopyrightText: 2026 Xaver Hugl <xaver.hugl@kde.org>

    SPDX-License-Identifier: GPL-2.0-or-later
*/

#include "scene/vulkan/ninepatch.h"
#include "main.h"
#include "vulkan/vulkan_device.h"
#include "vulkan/vulkan_texture.h"

#include <QPainter>

namespace KWin
{

std::unique_ptr<NinePatchVulkan> NinePatchVulkan::create(VulkanDevice *device, const QImage &image)
{
    auto texture = VulkanTexture::upload(device, image, vk::ImageUsageFlagBits::eSampled | vk::ImageUsageFlagBits::eTransferSrc);
    if (!texture) {
        return nullptr;
    }
    return std::make_unique<NinePatchVulkan>(std::move(texture));
}

std::unique_ptr<NinePatchVulkan> NinePatchVulkan::create(VulkanDevice *device,
                                                         const QImage &topLeftPatch,
                                                         const QImage &topPatch,
                                                         const QImage &topRightPatch,
                                                         const QImage &rightPatch,
                                                         const QImage &bottomRightPatch,
                                                         const QImage &bottomPatch,
                                                         const QImage &bottomLeftPatch,
                                                         const QImage &leftPatch)
{
    const QSize top(topPatch.size());
    const QSize topRight(topRightPatch.size());
    const QSize right(rightPatch.size());
    const QSize bottom(bottomPatch.size());
    const QSize bottomLeft(bottomLeftPatch.size());
    const QSize left(leftPatch.size());
    const QSize topLeft(topLeftPatch.size());
    const QSize bottomRight(bottomRightPatch.size());

    const int width = std::max({topLeft.width(), left.width(), bottomLeft.width()}) + std::max(top.width(), bottom.width()) + std::max({topRight.width(), right.width(), bottomRight.width()});
    const int height = std::max({topLeft.height(), top.height(), topRight.height()}) + std::max(left.height(), right.height()) + std::max({bottomLeft.height(), bottom.height(), bottomRight.height()});

    if (width == 0 || height == 0) {
        return nullptr;
    }

    QImage image(width, height, QImage::Format_ARGB32);
    image.fill(Qt::transparent);

    const int innerRectTop = std::max({topLeft.height(), top.height(), topRight.height()});
    const int innerRectLeft = std::max({topLeft.width(), left.width(), bottomLeft.width()});

    QPainter p;
    p.begin(&image);

    p.drawImage(QRectF(0, 0, topLeft.width(), topLeft.height()), topLeftPatch);
    p.drawImage(QRectF(innerRectLeft, 0, top.width(), top.height()), topPatch);
    p.drawImage(QRectF(width - topRight.width(), 0, topRight.width(), topRight.height()), topRightPatch);

    p.drawImage(QRectF(0, innerRectTop, left.width(), left.height()), leftPatch);
    p.drawImage(QRectF(width - right.width(), innerRectTop, right.width(), right.height()), rightPatch);

    p.drawImage(QRectF(0, height - bottomLeft.height(), bottomLeft.width(), bottomLeft.height()), bottomLeftPatch);
    p.drawImage(QRectF(innerRectLeft, height - bottom.height(), bottom.width(), bottom.height()), bottomPatch);
    p.drawImage(QRectF(width - bottomRight.width(), height - bottomRight.height(), bottomRight.width(), bottomRight.height()), bottomRightPatch);

    p.end();

    // TODO optimization for alpha-only textures

    auto texture = VulkanTexture::upload(device, image, vk::ImageUsageFlagBits::eSampled | vk::ImageUsageFlagBits::eTransferSrc);
    if (!texture) {
        return nullptr;
    }
    return std::make_unique<NinePatchVulkan>(std::move(texture));
}

NinePatchVulkan::NinePatchVulkan(std::unique_ptr<VulkanTexture> &&texture)
    : m_texture(std::move(texture))
{
}

NinePatchVulkan::~NinePatchVulkan()
{
}

VulkanTexture *NinePatchVulkan::texture() const
{
    return m_texture.get();
}

} // namespace KWin

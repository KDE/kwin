/*
    SPDX-FileCopyrightText: 2026 Vlad Zahorodnii <vlad.zahorodnii@kde.org>
    SPDX-FileCopyrightText: 2026 Xaver Hugl <xaver.hugl@kde.org>

    SPDX-License-Identifier: GPL-2.0-or-later
*/

#include "scene/vulkan/atlas.h"
#include "main.h"
#include "vulkan/vulkan_device.h"
#include "vulkan/vulkan_texture.h"

#include <QPainter>

namespace KWin
{

std::unique_ptr<AtlasVulkan> AtlasVulkan::create(VulkanDevice *device, const QList<QImage> &images)
{
    auto atlas = std::make_unique<AtlasVulkan>(device);
    if (atlas->reset(images)) {
        return atlas;
    }
    return nullptr;
}

AtlasVulkan::AtlasVulkan(VulkanDevice *device)
    : m_device(device)
{
}

AtlasVulkan::~AtlasVulkan()
{
}

Atlas::Sprite AtlasVulkan::sprite(uint spriteId) const
{
    return m_sprites.value(spriteId);
}

VulkanTexture *AtlasVulkan::texture() const
{
    return m_texture.get();
}

static int align(int value, int align)
{
    return (value + align - 1) & ~(align - 1);
}

static void clamp_row(int left, int width, int right, const uint32_t *src, uint32_t *dest)
{
    std::fill_n(dest, left, *src);
    std::copy(src, src + width, dest + left);
    std::fill_n(dest + left + width, right, *(src + width - 1));
}

static void clamp_sides(int left, int width, int right, const uint32_t *src, uint32_t *dest)
{
    std::fill_n(dest, left, *src);
    std::fill_n(dest + left + width, right, *(src + width - 1));
}

static void clamp(QImage &image, const Rect &viewport)
{
    Q_ASSERT(image.depth() == 32);
    if (viewport.isEmpty()) {
        image = {};
        return;
    }

    const Rect rect = image.rect();

    const int left = viewport.left() - rect.left();
    const int top = viewport.top() - rect.top();
    const int right = rect.right() - viewport.right();
    const int bottom = rect.bottom() - viewport.bottom();

    const int width = rect.width() - left - right;
    const int height = rect.height() - top - bottom;

    const uint32_t *firstRow = reinterpret_cast<uint32_t *>(image.scanLine(top));
    const uint32_t *lastRow = reinterpret_cast<uint32_t *>(image.scanLine(top + height - 1));

    for (int i = 0; i < top; ++i) {
        uint32_t *dest = reinterpret_cast<uint32_t *>(image.scanLine(i));
        clamp_row(left, width, right, firstRow + left, dest);
    }

    for (int i = 0; i < height; ++i) {
        uint32_t *dest = reinterpret_cast<uint32_t *>(image.scanLine(top + i));
        clamp_sides(left, width, right, dest + left, dest);
    }

    for (int i = 0; i < bottom; ++i) {
        uint32_t *dest = reinterpret_cast<uint32_t *>(image.scanLine(top + height + i));
        clamp_row(left, width, right, lastRow + left, dest);
    }
}

bool AtlasVulkan::update(uint sprite, const QImage &image, const Rect &damage)
{
    Q_ASSERT(!image.isNull());
    if (sprite >= m_sprites.size()) {
        return false;
    }

    upload(sprite, image, damage);
    return true;
}

static QMargins atlasSpritePadding(const QImage &sprite, const Rect &updateRect)
{
    const Rect spriteRect = sprite.rect();

    QMargins padding;
    if (spriteRect.top() == updateRect.top()) {
        padding.setTop(1);
    }
    if (spriteRect.right() == updateRect.right()) {
        padding.setRight(1);
    }
    if (spriteRect.bottom() == updateRect.bottom()) {
        padding.setBottom(1);
    }
    if (spriteRect.left() == updateRect.left()) {
        padding.setLeft(1);
    }
    return padding;
}

static QMargins rotatePadding(const QMargins &margins)
{
    return QMargins(margins.top(), margins.right(), margins.bottom(), margins.left());
}

void AtlasVulkan::upload(uint spriteId, const QImage &image, const Rect &damage)
{
    const Sprite &sprite = m_sprites[spriteId];

    const QMargins padding = atlasSpritePadding(image, damage);
    const QPoint paddedPosition = sprite.geometry.topLeft() - QPoint(padding.left(), padding.top());

    if (sprite.rotated) {
        const QMargins rotatedPadding = rotatePadding(padding);
        const Rect rotatedDamage(QPoint(damage.top(), image.width() - damage.right()),
                                 QPoint(damage.bottom(), image.width() - damage.left()));

        QImage rotatedAndPadded(damage.size().transposed().grownBy(rotatedPadding), image.format());
        const Rect outer = rotatedAndPadded.rect();
        const Rect inner = outer.marginsRemoved(rotatedPadding);

        QPainter painter(&rotatedAndPadded);
        painter.setCompositionMode(QPainter::CompositionMode_Source);
        painter.setClipRect(inner);
        painter.translate(rotatedPadding.left(), rotatedPadding.top());
        painter.translate(0, image.width());
        painter.rotate(-90);
        painter.translate(-damage.topLeft());
        painter.drawImage(image.rect(), image);
        painter.end();

        clamp(rotatedAndPadded, inner);

        m_texture->update(rotatedAndPadded, outer, paddedPosition + rotatedDamage.topLeft());
    } else {
        if (padding.isNull()) {
            m_texture->update(image, damage, paddedPosition);
        } else {
            QImage padded(damage.size().grownBy(padding), image.format());
            const Rect outer = padded.rect();
            const Rect inner = outer.marginsRemoved(padding);

            QPainter painter(&padded);
            painter.setCompositionMode(QPainter::CompositionMode_Source);
            painter.setClipRect(inner);
            painter.translate(padding.left(), padding.right());
            painter.translate(-damage.topLeft());
            painter.drawImage(image.rect(), image);
            painter.end();

            clamp(padded, inner);

            m_texture->update(padded, outer, paddedPosition + damage.topLeft());
        }
    }
}

bool AtlasVulkan::reset(const QList<QImage> &images)
{
    const QMargins padding(1, 1, 1, 1);

    // The layout algorithm is pretty simple: it arranges sprites from top to bottom. If there is
    // a vertical sprite, it will be rotated. At the moment, it is optimized for storing decorations.
    Rect atlasRect;
    QList<Sprite> sprites;
    QImage::Format imageFormat = QImage::Format_Invalid;
    for (const QImage &image : images) {
        if (image.isNull()) {
            sprites.append({
                .geometry = Rect(),
                .rotated = false,
            });
        } else {
            imageFormat = image.format();
            const bool rotated = image.width() < image.height();

            QSize outerSize = image.size().grownBy(padding);
            if (rotated) {
                outerSize.transpose();
            }

            const Rect outerRect = Rect(QPoint(0, atlasRect.bottom()), outerSize);
            const Rect innerRect = outerRect.shrunkBy(padding);

            sprites.append({
                .geometry = innerRect,
                .rotated = rotated,
            });

            atlasRect |= outerRect;
        }
    }

    const QSize textureSize(align(atlasRect.width(), 128), align(atlasRect.height(), 128));
    if (textureSize.isEmpty()) {
        m_texture.reset();
        m_sprites.clear();
        return false;
    }

    auto format = VulkanTexture::qImageToVulkanFormat(imageFormat);
    if (!format.has_value()) {
        return false;
    }
    if (!m_texture || m_texture->size() != textureSize || m_texture->format() != format->format || m_texture->swizzles() != format->swizzles) {
        m_texture = VulkanTexture::allocate(m_device, format->format, format->swizzles, textureSize, vk::ImageUsageFlagBits::eSampled | vk::ImageUsageFlagBits::eTransferDst);
        if (!m_texture) {
            m_sprites.clear();
            return false;
        }
    }

    m_sprites = sprites;

    for (int i = 0; i < images.size(); ++i) {
        if (!images[i].isNull()) {
            upload(i, images[i], images[i].rect());
        }
    }

    return true;
}

} // namespace KWin

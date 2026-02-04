/*
    SPDX-FileCopyrightText: 2026 Vlad Zahorodnii <vlad.zahorodnii@kde.org>

    SPDX-License-Identifier: GPL-2.0-or-later
*/

#include "scene/qpainter/atlas.h"

#include <QPainter>

namespace KWin
{

std::unique_ptr<AtlasQPainter> AtlasQPainter::create(const QList<QImage> &images)
{
    auto atlas = std::make_unique<AtlasQPainter>();
    atlas->reset(images);
    return atlas;
}

AtlasQPainter::Sprite AtlasQPainter::sprite(uint spriteId) const
{
    return m_sprites.value(spriteId);
}

QImage AtlasQPainter::image(uint spriteId) const
{
    return m_images.value(spriteId);
}

bool AtlasQPainter::update(uint spriteId, const QImage &image, const Rect &damage)
{
    if (spriteId >= m_images.size()) {
        return false;
    }

    m_images[spriteId] = image;
    return true;
}

bool AtlasQPainter::reset(const QList<QImage> &images)
{
    m_images = images;

    m_sprites.clear();
    for (const QImage &image : images) {
        m_sprites.append({
            .geometry = image.rect(),
            .rotated = false,
        });
    }

    return true;
}

} // namespace KWin

/*
    SPDX-FileCopyrightText: 2022 Vlad Zahorodnii <vlad.zahorodnii@kde.org>

    SPDX-License-Identifier: GPL-2.0-or-later
*/

#include "scene/imageitem.h"
#include "scene/itemrenderer.h"
#include "scene/scene.h"
#include "scene/texture.h"

namespace KWin
{

ImageItem::ImageItem(Item *parent)
    : Item(parent)
{
}

ImageItem::~ImageItem()
{
}

Texture *ImageItem::texture() const
{
    return m_texture.get();
}

QImage ImageItem::image() const
{
    return m_image;
}

void ImageItem::setImage(const QImage &image)
{
    m_image = image;
    discardQuads();
    scheduleRepaint(boundingRect());
}

void ImageItem::preprocess()
{
    if (m_image.isNull()) {
        m_texture.reset();
        m_textureKey = 0;
    } else if (!m_texture || m_textureKey != m_image.cacheKey()) {
        m_textureKey = m_image.cacheKey();

        ItemRenderer *itemRenderer = scene()->renderer();
        if (!m_texture || m_texture->size() != m_image.size()) {
            m_texture = itemRenderer->createTexture(m_image);
        } else {
            m_texture->upload(m_image, m_image.rect());
        }
    }
}

WindowQuadList ImageItem::buildQuads() const
{
    const RectF geometry = boundingRect();
    if (geometry.isEmpty()) {
        return WindowQuadList{};
    }

    const RectF imageRect = m_image.rect();

    WindowQuad quad;
    quad[0] = WindowVertex(geometry.topLeft(), imageRect.topLeft());
    quad[1] = WindowVertex(geometry.topRight(), imageRect.topRight());
    quad[2] = WindowVertex(geometry.bottomRight(), imageRect.bottomRight());
    quad[3] = WindowVertex(geometry.bottomLeft(), imageRect.bottomLeft());

    WindowQuadList ret;
    ret.append(quad);
    return ret;
}

void ImageItem::releaseResources()
{
    m_texture.reset();
}

} // namespace KWin

#include "moc_imageitem.cpp"

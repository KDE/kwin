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

Texture *ImageItem::texture(RenderDevice *device) const
{
    const auto it = m_textures.find(device);
    return it == m_textures.end() ? nullptr : it->second.first.get();
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

void ImageItem::preprocess(ItemRenderer *renderer)
{
    auto &[texture, key] = m_textures[renderer->renderDevice()];
    if (m_image.isNull()) {
        texture.reset();
        key = 0;
    } else if (!texture || key != m_image.cacheKey()) {
        key = m_image.cacheKey();

        if (!texture || texture->size() != m_image.size()) {
            texture = renderer->createTexture(m_image);
        } else {
            texture->upload(m_image, m_image.rect());
        }
    }
}

WindowQuadList ImageItem::buildQuads(ItemRenderer *renderer) const
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

void ImageItem::releaseResources(RenderDevice *device)
{
    m_textures.erase(device);
}

} // namespace KWin

#include "moc_imageitem.cpp"

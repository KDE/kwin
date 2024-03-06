/*
    SPDX-FileCopyrightText: 2022 Vlad Zahorodnii <vlad.zahorodnii@kde.org>

    SPDX-License-Identifier: GPL-2.0-or-later
*/

#include "scene/cursoritem.h"
#include "cursor.h"
#include "cursorsource.h"
#include "effect/effect.h"
#include "scene/imageitem.h"
#include "scene/itemrenderer.h"
#include "scene/scene.h"
#include "scene/surfaceitem_wayland.h"

namespace KWin
{

CursorItem::CursorItem(Item *parent)
    : Item(parent)
{
    refresh();
    connect(Cursors::self(), &Cursors::currentCursorChanged, this, &CursorItem::refresh);
}

CursorItem::~CursorItem()
{
}

void CursorItem::refresh()
{
    const CursorSource *source = Cursors::self()->currentCursor()->source();
    if (auto surfaceSource = qobject_cast<const SurfaceCursorSource *>(source)) {
        setSurface(surfaceSource->surface(), surfaceSource->hotspot());
    } else if (auto shapeSource = qobject_cast<const ShapeCursorSource *>(source)) {
        setImage(shapeSource->image(), shapeSource->hotspot());
    }
}

void CursorItem::setSurface(SurfaceInterface *surface, const QPointF &hotspot)
{
    m_imageItem.reset();

    if (!m_surfaceItem || m_surfaceItem->surface() != surface) {
        if (surface) {
            m_surfaceItem = std::make_unique<SurfaceItemWayland>(surface, this);
        } else {
            m_surfaceItem.reset();
        }
    }
    if (m_surfaceItem) {
        m_surfaceItem->setPosition(-hotspot);
    }
}

void CursorItem::setImage(const QImage &image, const QPointF &hotspot)
{
    m_surfaceItem.reset();

    if (!m_imageItem) {
        m_imageItem = scene()->renderer()->createImageItem(this);
    }
    m_imageItem->setImage(image);
    m_imageItem->setPosition(-hotspot);
    m_imageItem->setSize(image.size() / image.devicePixelRatio());
}

} // namespace KWin

#include "moc_cursoritem.cpp"

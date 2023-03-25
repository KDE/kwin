/*
    SPDX-FileCopyrightText: 2022 Vlad Zahorodnii <vlad.zahorodnii@kde.org>

    SPDX-License-Identifier: GPL-2.0-or-later
*/

#include "scene/cursoritem.h"
#include "cursor.h"
#include "cursorsource.h"
#include "scene/imageitem.h"
#include "scene/itemrenderer.h"
#include "scene/scene.h"
#include "scene/surfaceitem_wayland.h"
#include "wayland/surface_interface.h"
#include "wayland_server.h"

namespace KWin
{

CursorItem::CursorItem(Scene *scene, Item *parent)
    : Item(scene, parent)
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
        // TODO Plasma 6: Stop setting XCURSOR_SIZE and scale Xcursor.size in xrdb.
        if (surfaceSource->surface() && surfaceSource->surface()->client() == waylandServer()->xWaylandConnection()) {
            setImage(surfaceSource->image());
        } else {
            setSurface(surfaceSource->surface());
        }
    } else if (auto imageSource = qobject_cast<const ImageCursorSource *>(source)) {
        setImage(imageSource->image());
    } else if (auto shapeSource = qobject_cast<const ShapeCursorSource *>(source)) {
        setImage(shapeSource->image());
    }
}

void CursorItem::setSurface(KWaylandServer::SurfaceInterface *surface)
{
    m_imageItem.reset();

    if (!m_surfaceItem || m_surfaceItem->surface() != surface) {
        if (surface) {
            m_surfaceItem = std::make_unique<SurfaceItemWayland>(surface, scene(), this);
        } else {
            m_surfaceItem.reset();
        }
    }
}

void CursorItem::setImage(const QImage &image)
{
    m_surfaceItem.reset();

    if (!m_imageItem) {
        m_imageItem.reset(scene()->renderer()->createImageItem(scene(), this));
    }
    m_imageItem->setImage(image);
    m_imageItem->setSize(image.size() / image.devicePixelRatio());
}

} // namespace KWin

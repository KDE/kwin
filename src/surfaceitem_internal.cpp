/*
    SPDX-FileCopyrightText: 2021 Vlad Zahorodnii <vlad.zahorodnii@kde.org>

    SPDX-License-Identifier: GPL-2.0-or-later
*/

#include "surfaceitem_internal.h"
#include "composite.h"
#include "internal_client.h"
#include "scene.h"

namespace KWin
{

SurfaceItemInternal::SurfaceItemInternal(Scene::Window *window, Item *parent)
    : SurfaceItem(window, parent)
{
    Toplevel *toplevel = window->window();

    connect(toplevel, &Toplevel::bufferGeometryChanged,
            this, &SurfaceItemInternal::handleBufferGeometryChanged);

    setSize(toplevel->bufferGeometry().size());
}

QPointF SurfaceItemInternal::mapToBuffer(const QPointF &point) const
{
    return point * window()->window()->bufferScale();
}

QRegion SurfaceItemInternal::shape() const
{
    return QRegion(0, 0, width(), height());
}

SurfacePixmap *SurfaceItemInternal::createPixmap()
{
    return new SurfacePixmapInternal(this);
}

void SurfaceItemInternal::handleBufferGeometryChanged(Toplevel *toplevel, const QRect &old)
{
    if (toplevel->bufferGeometry().size() != old.size()) {
        discardPixmap();
    }
    setSize(toplevel->bufferGeometry().size());
}

SurfacePixmapInternal::SurfacePixmapInternal(SurfaceItemInternal *item, QObject *parent)
    : SurfacePixmap(Compositor::self()->scene()->createPlatformSurfaceTextureInternal(this), parent)
    , m_item(item)
{
}

ClientBufferRef SurfacePixmapInternal::buffer() const
{
    return m_bufferRef;
}

void SurfacePixmapInternal::create()
{
    update();
}

void SurfacePixmapInternal::update()
{
    const InternalClient *client = qobject_cast<InternalClient *>(m_item->window()->window());
    if (client) {
        m_bufferRef = client->buffer();
        m_hasAlphaChannel = m_bufferRef.hasAlphaChannel();
    }
}

bool SurfacePixmapInternal::isValid() const
{
    return m_bufferRef;
}

} // namespace KWin

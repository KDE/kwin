/*
    SPDX-FileCopyrightText: 2021 Vlad Zahorodnii <vlad.zahorodnii@kde.org>

    SPDX-License-Identifier: GPL-2.0-or-later
*/

#include "surfaceitem_wayland.h"

#include <KWaylandServer/subcompositor_interface.h>
#include <KWaylandServer/surface_interface.h>

namespace KWin
{

SurfaceItemWayland::SurfaceItemWayland(KWaylandServer::SurfaceInterface *surface,
                                       Scene::Window *window, Item *parent)
    : SurfaceItem(window, parent)
    , m_surface(surface)
{
    connect(surface, &KWaylandServer::SurfaceInterface::surfaceToBufferMatrixChanged,
            this, &SurfaceItemWayland::discardQuads);
    connect(surface, &KWaylandServer::SurfaceInterface::surfaceToBufferMatrixChanged,
            this, &SurfaceItemWayland::discardPixmap);

    connect(surface, &KWaylandServer::SurfaceInterface::sizeChanged,
            this, &SurfaceItemWayland::handleSurfaceSizeChanged);
    connect(surface, &KWaylandServer::SurfaceInterface::bufferSizeChanged,
            this, &SurfaceItemWayland::discardPixmap);

    connect(surface, &KWaylandServer::SurfaceInterface::childSubSurfacesChanged,
            this, &SurfaceItemWayland::handleChildSubSurfacesChanged);
    connect(surface, &KWaylandServer::SurfaceInterface::committed,
            this, &SurfaceItemWayland::handleSurfaceCommitted);
    connect(surface, &KWaylandServer::SurfaceInterface::damaged,
            this, &SurfaceItemWayland::addDamage);
    connect(surface, &KWaylandServer::SurfaceInterface::childSubSurfaceAdded,
            this, &SurfaceItemWayland::handleChildSubSurfaceAdded);
    connect(surface, &KWaylandServer::SurfaceInterface::childSubSurfaceRemoved,
            this, &SurfaceItemWayland::handleChildSubSurfaceRemoved);

    KWaylandServer::SubSurfaceInterface *subsurface = surface->subSurface();
    if (subsurface) {
        connect(subsurface, &KWaylandServer::SubSurfaceInterface::positionChanged,
                this, &SurfaceItemWayland::handleSubSurfacePositionChanged);
        setPosition(subsurface->position());
    }

    const QList<KWaylandServer::SubSurfaceInterface *> children = surface->childSubSurfaces();
    for (KWaylandServer::SubSurfaceInterface *subsurface : children) {
        handleChildSubSurfaceAdded(subsurface);
    }

    setSize(surface->size());
}

QPointF SurfaceItemWayland::mapToBuffer(const QPointF &point) const
{
    if (m_surface) {
        return m_surface->mapToBuffer(point);
    }
    return point;
}

QRegion SurfaceItemWayland::shape() const
{
    return QRegion(0, 0, width(), height());
}

QRegion SurfaceItemWayland::opaque() const
{
    if (m_surface) {
        return m_surface->opaque();
    }
    return QRegion();
}

KWaylandServer::SurfaceInterface *SurfaceItemWayland::surface() const
{
    return m_surface;
}

void SurfaceItemWayland::handleSurfaceSizeChanged()
{
    setSize(m_surface->size());
}

void SurfaceItemWayland::handleSurfaceCommitted()
{
    if (m_surface->hasFrameCallbacks()) {
        scheduleRepaint();
    }
}

void SurfaceItemWayland::handleChildSubSurfaceAdded(KWaylandServer::SubSurfaceInterface *child)
{
    SurfaceItemWayland *subsurfaceItem = new SurfaceItemWayland(child->surface(), window());
    subsurfaceItem->setParent(this);
    subsurfaceItem->setParentItem(this);

    m_subsurfaces.insert(child, subsurfaceItem);
}

void SurfaceItemWayland::handleChildSubSurfaceRemoved(KWaylandServer::SubSurfaceInterface *child)
{
    delete m_subsurfaces.take(child);
}

void SurfaceItemWayland::handleChildSubSurfacesChanged()
{
    const QList<KWaylandServer::SubSurfaceInterface *> stackingOrder = m_surface->childSubSurfaces();
    QList<Item *> items;
    items.reserve(stackingOrder.count());

    for (KWaylandServer::SubSurfaceInterface *subsurface : stackingOrder) {
        items.append(m_subsurfaces[subsurface]);
    }

    stackChildren(items);
}

void SurfaceItemWayland::handleSubSurfacePositionChanged()
{
    setPosition(m_surface->subSurface()->position());
}

WindowPixmap *SurfaceItemWayland::createPixmap()
{
    WindowPixmap *pixmap = window()->createWindowPixmap();
    pixmap->setSurface(m_surface);
    return pixmap;
}

SurfaceItemXwayland::SurfaceItemXwayland(Scene::Window *window, Item *parent)
    : SurfaceItemWayland(window->window()->surface(), window, parent)
{
    const Toplevel *toplevel = window->window();
    connect(toplevel, &Toplevel::geometryShapeChanged, this, &SurfaceItemXwayland::discardQuads);
}

QRegion SurfaceItemXwayland::shape() const
{
    const Toplevel *toplevel = window()->window();
    if (window()->isShaded()) {
        return QRegion();
    }

    const QRect clipRect = toplevel->clientGeometry().translated(-toplevel->bufferGeometry().topLeft());
    const QRegion shape = toplevel->shapeRegion();

    return shape & clipRect;
}

} // namespace KWin

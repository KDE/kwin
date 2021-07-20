/*
    SPDX-FileCopyrightText: 2021 Vlad Zahorodnii <vlad.zahorodnii@kde.org>

    SPDX-License-Identifier: GPL-2.0-or-later
*/

#include "surfaceitem_wayland.h"
#include "composite.h"
#include "scene.h"

#include <KWaylandServer/clientbuffer.h>
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
            this, &SurfaceItemWayland::handleSurfaceToBufferMatrixChanged);

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
    connect(surface, &KWaylandServer::SurfaceInterface::childSubSurfaceRemoved,
            this, &SurfaceItemWayland::handleChildSubSurfaceRemoved);

    KWaylandServer::SubSurfaceInterface *subsurface = surface->subSurface();
    if (subsurface) {
        connect(subsurface, &KWaylandServer::SubSurfaceInterface::positionChanged,
                this, &SurfaceItemWayland::handleSubSurfacePositionChanged);
        setPosition(subsurface->position());
    }

    handleChildSubSurfacesChanged();
    setSize(surface->size());
    setSurfaceToBufferMatrix(surface->surfaceToBufferMatrix());
}

QRegion SurfaceItemWayland::shape() const
{
    return QRegion(rect());
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

void SurfaceItemWayland::handleSurfaceToBufferMatrixChanged()
{
    setSurfaceToBufferMatrix(m_surface->surfaceToBufferMatrix());
    discardQuads();
    discardPixmap();
}

void SurfaceItemWayland::handleSurfaceSizeChanged()
{
    setSize(m_surface->size());
}

void SurfaceItemWayland::handleSurfaceCommitted()
{
    if (m_surface->hasFrameCallbacks()) {
        scheduleFrame();
    }
}

SurfaceItemWayland *SurfaceItemWayland::getOrCreateSubSurfaceItem(KWaylandServer::SubSurfaceInterface *child)
{
    SurfaceItemWayland *&item = m_subsurfaces[child];
    if (!item) {
        item = new SurfaceItemWayland(child->surface(), window());
        item->setParent(this);
        item->setParentItem(this);
    }
    return item;
}

void SurfaceItemWayland::handleChildSubSurfaceRemoved(KWaylandServer::SubSurfaceInterface *child)
{
    delete m_subsurfaces.take(child);
}

void SurfaceItemWayland::handleChildSubSurfacesChanged()
{
    const QList<KWaylandServer::SubSurfaceInterface *> below = m_surface->below();
    const QList<KWaylandServer::SubSurfaceInterface *> above = m_surface->above();

    for (int i = 0; i < below.count(); ++i) {
        SurfaceItemWayland *subsurfaceItem = getOrCreateSubSurfaceItem(below[i]);
        subsurfaceItem->setZ(i - below.count());
    }

    for (int i = 0; i < above.count(); ++i) {
        SurfaceItemWayland *subsurfaceItem = getOrCreateSubSurfaceItem(above[i]);
        subsurfaceItem->setZ(i);
    }
}

void SurfaceItemWayland::handleSubSurfacePositionChanged()
{
    setPosition(m_surface->subSurface()->position());
}

SurfacePixmap *SurfaceItemWayland::createPixmap()
{
    return new SurfacePixmapWayland(this);
}

SurfacePixmapWayland::SurfacePixmapWayland(SurfaceItemWayland *item, QObject *parent)
    : SurfacePixmap(Compositor::self()->scene()->createPlatformSurfaceTextureWayland(this), parent)
    , m_item(item)
{
}

SurfacePixmapWayland::~SurfacePixmapWayland()
{
    setBuffer(nullptr);
}

SurfaceItemWayland *SurfacePixmapWayland::item() const
{
    return m_item;
}

KWaylandServer::SurfaceInterface *SurfacePixmapWayland::surface() const
{
    return m_item->surface();
}

KWaylandServer::ClientBuffer *SurfacePixmapWayland::buffer() const
{
    return m_buffer;
}

void SurfacePixmapWayland::create()
{
    update();
}

void SurfacePixmapWayland::update()
{
    KWaylandServer::SurfaceInterface *surface = m_item->surface();
    if (surface) {
        setBuffer(surface->buffer());
    }
}

bool SurfacePixmapWayland::isValid() const
{
    return m_buffer;
}

void SurfacePixmapWayland::setBuffer(KWaylandServer::ClientBuffer *buffer)
{
    if (m_buffer == buffer) {
        return;
    }
    if (m_buffer) {
        m_buffer->unref();
    }
    m_buffer = buffer;
    if (m_buffer) {
        m_buffer->ref();
        m_hasAlphaChannel = m_buffer->hasAlphaChannel();
    }
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
    const QRect clipRect = rect() & toplevel->clientGeometry().translated(-toplevel->bufferGeometry().topLeft());
    const QRegion shape = toplevel->shapeRegion();

    return shape & clipRect;
}

} // namespace KWin

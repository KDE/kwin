/*
    SPDX-FileCopyrightText: 2021 Vlad Zahorodnii <vlad.zahorodnii@kde.org>

    SPDX-License-Identifier: GPL-2.0-or-later
*/

#include "surface_wayland.h"
#include "composite.h"
#include "deleted.h"
#include "scene.h"
#include "toplevel.h"

#include <KWaylandServer/clientbuffer.h>
#include <KWaylandServer/subcompositor_interface.h>
#include <KWaylandServer/surface_interface.h>

namespace KWin
{

SurfaceWayland::SurfaceWayland(KWaylandServer::SurfaceInterface *surface, Toplevel *window, QObject *parent)
    : Surface(window, parent)
    , m_surface(surface)
{
    connect(surface, &KWaylandServer::SurfaceInterface::surfaceToBufferMatrixChanged,
            this, &SurfaceWayland::handleSurfaceToBufferMatrixChanged);

    connect(surface, &KWaylandServer::SurfaceInterface::sizeChanged,
            this, &SurfaceWayland::handleSurfaceSizeChanged);
    connect(surface, &KWaylandServer::SurfaceInterface::bufferSizeChanged,
            this, &SurfaceWayland::discardPixmap);

    connect(surface, &KWaylandServer::SurfaceInterface::childSubSurfacesChanged,
            this, &SurfaceWayland::handleChildSubSurfacesChanged);
    connect(surface, &KWaylandServer::SurfaceInterface::committed,
            this, &SurfaceWayland::handleSurfaceCommitted);
    connect(surface, &KWaylandServer::SurfaceInterface::damaged,
            this, &SurfaceWayland::damaged);
    connect(surface, &KWaylandServer::SurfaceInterface::childSubSurfaceRemoved,
            this, &SurfaceWayland::handleChildSubSurfaceRemoved);

    KWaylandServer::SubSurfaceInterface *subsurface = surface->subSurface();
    if (subsurface) {
        connect(surface, &KWaylandServer::SurfaceInterface::mapped,
                this, &SurfaceWayland::handleSubSurfaceMappedChanged);
        connect(surface, &KWaylandServer::SurfaceInterface::unmapped,
                this, &SurfaceWayland::handleSubSurfaceMappedChanged);
        connect(subsurface, &KWaylandServer::SubSurfaceInterface::positionChanged,
                this, &SurfaceWayland::handleSubSurfacePositionChanged);
        setMapped(surface->isMapped());
        setPosition(subsurface->position());
    }

    handleChildSubSurfacesChanged();
    setSize(surface->size());
    setSurfaceToBufferMatrix(surface->surfaceToBufferMatrix());
}

SurfacePixmap *SurfaceWayland::createPixmap()
{
    return new SurfacePixmapWayland(this);
}

QRegion SurfaceWayland::shape() const
{
    return QRegion(QRect(QPoint(0, 0), size()));
}

QRegion SurfaceWayland::opaque() const
{
    if (m_surface) {
        return m_surface->opaque();
    }
    return QRegion();
}

KWaylandServer::SurfaceInterface *SurfaceWayland::surface() const
{
    return m_surface;
}

void SurfaceWayland::handleSurfaceToBufferMatrixChanged()
{
    setSurfaceToBufferMatrix(m_surface->surfaceToBufferMatrix());
    discardQuads();
    discardPixmap();
}

void SurfaceWayland::handleSurfaceSizeChanged()
{
    setSize(m_surface->size());
}

void SurfaceWayland::handleSurfaceCommitted()
{
    if (m_surface->hasFrameCallbacks()) {
        frame();
    }
}

SurfaceWayland *SurfaceWayland::getOrCreateChild(KWaylandServer::SubSurfaceInterface *subsurface)
{
    SurfaceWayland *&child = m_subsurfaces[subsurface];
    if (!child) {
        child = new SurfaceWayland(subsurface->surface(), window(), this);
    }
    return child;
}

void SurfaceWayland::handleChildSubSurfaceRemoved(KWaylandServer::SubSurfaceInterface *child)
{
    delete m_subsurfaces.take(child);
}

void SurfaceWayland::handleChildSubSurfacesChanged()
{
    const QList<KWaylandServer::SubSurfaceInterface *> belowSubsurfaces = m_surface->below();
    const QList<KWaylandServer::SubSurfaceInterface *> aboveSubsurfaces = m_surface->above();

    QList<Surface *> below;
    QList<Surface *> above;

    below.reserve(belowSubsurfaces.count());
    above.reserve(aboveSubsurfaces.count());

    for (int i = 0; i < belowSubsurfaces.count(); ++i) {
        below.append(getOrCreateChild(belowSubsurfaces[i]));
    }
    for (int i = 0; i < aboveSubsurfaces.count(); ++i) {
        above.append(getOrCreateChild(aboveSubsurfaces[i]));
    }

    setBelow(below);
    setAbove(above);
}

void SurfaceWayland::handleSubSurfacePositionChanged()
{
    setPosition(m_surface->subSurface()->position());
}

void SurfaceWayland::handleSubSurfaceMappedChanged()
{
    setMapped(m_surface->isMapped());
}

SurfacePixmapWayland::SurfacePixmapWayland(SurfaceWayland *item, QObject *parent)
    : SurfacePixmap(Compositor::self()->scene()->createSurfaceTextureWayland(this), parent)
    , m_item(item)
{
}

SurfacePixmapWayland::~SurfacePixmapWayland()
{
    setBuffer(nullptr);
}

SurfaceWayland *SurfacePixmapWayland::item() const
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

SurfaceXwayland::SurfaceXwayland(Toplevel *window, QObject *parent)
    : SurfaceWayland(window->surface(), window, parent)
{
    connect(window, &Toplevel::geometryShapeChanged, this, &SurfaceXwayland::discardQuads);
}

QRegion SurfaceXwayland::shape() const
{
    const QRect clipRect = QRect(QPoint(0, 0), size()) & window()->clientGeometry().translated(-window()->bufferGeometry().topLeft());
    const QRegion shape = window()->shapeRegion();

    return shape & clipRect;
}

} // namespace KWin

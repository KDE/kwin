/*
    SPDX-FileCopyrightText: 2021 Vlad Zahorodnii <vlad.zahorodnii@kde.org>

    SPDX-License-Identifier: GPL-2.0-or-later
*/

#include "scene/surfaceitem_wayland.h"
#include "composite.h"
#include "core/renderbackend.h"
#include "deleted.h"
#include "wayland/clientbuffer.h"
#include "wayland/subcompositor_interface.h"
#include "wayland/surface_interface.h"

namespace KWin
{

SurfaceItemWayland::SurfaceItemWayland(KWaylandServer::SurfaceInterface *surface, Scene *scene, Item *parent)
    : SurfaceItem(scene, parent)
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
        connect(surface, &KWaylandServer::SurfaceInterface::mapped,
                this, &SurfaceItemWayland::handleSubSurfaceMappedChanged);
        connect(surface, &KWaylandServer::SurfaceInterface::unmapped,
                this, &SurfaceItemWayland::handleSubSurfaceMappedChanged);
        connect(subsurface, &KWaylandServer::SubSurfaceInterface::positionChanged,
                this, &SurfaceItemWayland::handleSubSurfacePositionChanged);
        setVisible(surface->isMapped());
        setPosition(subsurface->position());
    }

    handleChildSubSurfacesChanged();
    setSize(surface->size());
    setSurfaceToBufferMatrix(surface->surfaceToBufferMatrix());
}

QVector<QRectF> SurfaceItemWayland::shape() const
{
    return {rect()};
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
        item = new SurfaceItemWayland(child->surface(), scene());
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

void SurfaceItemWayland::handleSubSurfaceMappedChanged()
{
    setVisible(m_surface->isMapped());
}

std::unique_ptr<SurfacePixmap> SurfaceItemWayland::createPixmap()
{
    return std::make_unique<SurfacePixmapWayland>(this);
}

ContentType SurfaceItemWayland::contentType() const
{
    return m_surface->contentType();
}

SurfacePixmapWayland::SurfacePixmapWayland(SurfaceItemWayland *item, QObject *parent)
    : SurfacePixmap(Compositor::self()->backend()->createSurfaceTextureWayland(this), parent)
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
        m_size = m_buffer->size();
    }
}

SurfaceItemXwayland::SurfaceItemXwayland(Window *window, Scene *scene, Item *parent)
    : SurfaceItemWayland(window->surface(), scene, parent)
    , m_window(window)
{
    connect(window, &Window::geometryShapeChanged, this, &SurfaceItemXwayland::discardQuads);
    connect(window, &Window::windowClosed, this, &SurfaceItemXwayland::handleWindowClosed);
}

QVector<QRectF> SurfaceItemXwayland::shape() const
{
    const QRectF clipRect = rect() & m_window->clientGeometry().translated(-m_window->bufferGeometry().topLeft());
    QVector<QRectF> shape = m_window->shapeRegion();

    // bounded to clipRect
    for (QRectF &shapePart : shape) {
        shapePart = shapePart.intersected(clipRect);
    }
    return shape;
}

void SurfaceItemXwayland::handleWindowClosed(Window *original, Deleted *deleted)
{
    m_window = deleted;
}

} // namespace KWin

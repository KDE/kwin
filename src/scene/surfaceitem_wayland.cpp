/*
    SPDX-FileCopyrightText: 2021 Vlad Zahorodnii <vlad.zahorodnii@kde.org>

    SPDX-License-Identifier: GPL-2.0-or-later
*/

#include "scene/surfaceitem_wayland.h"
#include "compositor.h"
#include "core/graphicsbuffer.h"
#include "core/renderbackend.h"
#include "wayland/subcompositor.h"
#include "wayland/surface.h"
#include "window.h"
#include "x11window.h"

namespace KWin
{

SurfaceItemWayland::SurfaceItemWayland(SurfaceInterface *surface, Scene *scene, Item *parent)
    : SurfaceItem(scene, parent)
    , m_surface(surface)
{
    connect(surface, &SurfaceInterface::sizeChanged,
            this, &SurfaceItemWayland::handleSurfaceSizeChanged);
    connect(surface, &SurfaceInterface::bufferSizeChanged,
            this, &SurfaceItemWayland::handleBufferSizeChanged);
    connect(surface, &SurfaceInterface::bufferSourceBoxChanged,
            this, &SurfaceItemWayland::handleBufferSourceBoxChanged);
    connect(surface, &SurfaceInterface::bufferTransformChanged,
            this, &SurfaceItemWayland::handleBufferTransformChanged);

    connect(surface, &SurfaceInterface::childSubSurfacesChanged,
            this, &SurfaceItemWayland::handleChildSubSurfacesChanged);
    connect(surface, &SurfaceInterface::committed,
            this, &SurfaceItemWayland::handleSurfaceCommitted);
    connect(surface, &SurfaceInterface::damaged,
            this, &SurfaceItemWayland::addDamage);
    connect(surface, &SurfaceInterface::childSubSurfaceRemoved,
            this, &SurfaceItemWayland::handleChildSubSurfaceRemoved);
    connect(surface, &SurfaceInterface::colorDescriptionChanged,
            this, &SurfaceItemWayland::handleColorDescriptionChanged);
    connect(surface, &SurfaceInterface::presentationModeHintChanged,
            this, &SurfaceItemWayland::handlePresentationModeHintChanged);

    SubSurfaceInterface *subsurface = surface->subSurface();
    if (subsurface) {
        connect(surface, &SurfaceInterface::mapped,
                this, &SurfaceItemWayland::handleSubSurfaceMappedChanged);
        connect(surface, &SurfaceInterface::unmapped,
                this, &SurfaceItemWayland::handleSubSurfaceMappedChanged);
        connect(subsurface, &SubSurfaceInterface::positionChanged,
                this, &SurfaceItemWayland::handleSubSurfacePositionChanged);
        setVisible(surface->isMapped());
        setPosition(subsurface->position());
    }

    handleChildSubSurfacesChanged();
    setDestinationSize(surface->size());
    setBufferTransform(surface->bufferTransform());
    setBufferSourceBox(surface->bufferSourceBox());
    setBufferSize(surface->bufferSize());
    setColorDescription(surface->colorDescription());
}

QList<QRectF> SurfaceItemWayland::shape() const
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

SurfaceInterface *SurfaceItemWayland::surface() const
{
    return m_surface;
}

void SurfaceItemWayland::handleSurfaceSizeChanged()
{
    setDestinationSize(m_surface->size());
}

void SurfaceItemWayland::handleBufferSizeChanged()
{
    setBufferSize(m_surface->bufferSize());
}

void SurfaceItemWayland::handleBufferSourceBoxChanged()
{
    setBufferSourceBox(m_surface->bufferSourceBox());
}

void SurfaceItemWayland::handleBufferTransformChanged()
{
    setBufferTransform(m_surface->bufferTransform());
}

void SurfaceItemWayland::handleSurfaceCommitted()
{
    if (m_surface->hasFrameCallbacks()) {
        scheduleFrame();
    }
}

SurfaceItemWayland *SurfaceItemWayland::getOrCreateSubSurfaceItem(SubSurfaceInterface *child)
{
    auto &item = m_subsurfaces[child];
    if (!item) {
        item = std::make_unique<SurfaceItemWayland>(child->surface(), scene());
        item->setParentItem(this);
    }
    return item.get();
}

void SurfaceItemWayland::handleChildSubSurfaceRemoved(SubSurfaceInterface *child)
{
    m_subsurfaces.erase(child);
}

void SurfaceItemWayland::handleChildSubSurfacesChanged()
{
    const QList<SubSurfaceInterface *> below = m_surface->below();
    const QList<SubSurfaceInterface *> above = m_surface->above();

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
    return m_surface ? m_surface->contentType() : ContentType::None;
}

void SurfaceItemWayland::freeze()
{
    if (!m_surface) {
        return;
    }

    m_surface->disconnect(this);
    if (auto subsurface = m_surface->subSurface()) {
        subsurface->disconnect(this);
    }

    for (auto &[subsurface, subsurfaceItem] : m_subsurfaces) {
        subsurfaceItem->freeze();
    }
}

void SurfaceItemWayland::handleColorDescriptionChanged()
{
    setColorDescription(m_surface->colorDescription());
}

void SurfaceItemWayland::handlePresentationModeHintChanged()
{
    setPresentationHint(m_surface->presentationModeHint());
}

SurfacePixmapWayland::SurfacePixmapWayland(SurfaceItemWayland *item, QObject *parent)
    : SurfacePixmap(Compositor::self()->backend()->createSurfaceTextureWayland(this), parent)
    , m_item(item)
{
}

void SurfacePixmapWayland::create()
{
    update();
}

void SurfacePixmapWayland::update()
{
    SurfaceInterface *surface = m_item->surface();
    if (surface) {
        setBuffer(surface->buffer());
    }
}

bool SurfacePixmapWayland::isValid() const
{
    return m_bufferRef;
}

SurfaceItemXwayland::SurfaceItemXwayland(X11Window *window, Scene *scene, Item *parent)
    : SurfaceItemWayland(window->surface(), scene, parent)
    , m_window(window)
{
    connect(window, &X11Window::shapeChanged, this, &SurfaceItemXwayland::discardQuads);
}

QList<QRectF> SurfaceItemXwayland::shape() const
{
    QList<QRectF> shape = m_window->shapeRegion();
    for (QRectF &shapePart : shape) {
        shapePart = shapePart.intersected(rect());
    }
    return shape;
}

QRegion SurfaceItemXwayland::opaque() const
{
    QRegion shapeRegion;
    for (const QRectF &shapePart : shape()) {
        shapeRegion += shapePart.toRect();
    }
    if (!m_window->hasAlpha()) {
        return shapeRegion;
    } else {
        return m_window->opaqueRegion() & shapeRegion;
    }
    return QRegion();
}

} // namespace KWin

#include "moc_surfaceitem_wayland.cpp"

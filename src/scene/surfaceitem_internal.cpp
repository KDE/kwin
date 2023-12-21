/*
    SPDX-FileCopyrightText: 2021 Vlad Zahorodnii <vlad.zahorodnii@kde.org>

    SPDX-License-Identifier: GPL-2.0-or-later
*/

#include "scene/surfaceitem_internal.h"
#include "compositor.h"
#include "core/renderbackend.h"
#include "internalwindow.h"

namespace KWin
{

SurfaceItemInternal::SurfaceItemInternal(InternalWindow *window, Scene *scene, Item *parent)
    : SurfaceItem(scene, parent)
    , m_window(window)
{
    connect(window, &Window::bufferGeometryChanged,
            this, &SurfaceItemInternal::handleBufferGeometryChanged);

    setDestinationSize(window->bufferGeometry().size());
    setBufferSourceBox(QRectF(QPointF(0, 0), window->bufferGeometry().size() * window->bufferScale()));
    setBufferSize((window->bufferGeometry().size() * window->bufferScale()).toSize());
}

InternalWindow *SurfaceItemInternal::window() const
{
    return m_window;
}

QList<QRectF> SurfaceItemInternal::shape() const
{
    return {rect()};
}

std::unique_ptr<SurfacePixmap> SurfaceItemInternal::createPixmap()
{
    return std::make_unique<SurfacePixmapInternal>(this);
}

void SurfaceItemInternal::handleBufferGeometryChanged()
{
    setDestinationSize(m_window->bufferGeometry().size());
    setBufferSourceBox(QRectF(QPointF(0, 0), m_window->bufferGeometry().size() * m_window->bufferScale()));
    setBufferSize((m_window->bufferGeometry().size() * m_window->bufferScale()).toSize());
}

SurfacePixmapInternal::SurfacePixmapInternal(SurfaceItemInternal *item, QObject *parent)
    : SurfacePixmap(Compositor::self()->backend()->createSurfaceTextureWayland(this), parent)
    , m_item(item)
{
}

void SurfacePixmapInternal::create()
{
    update();
}

void SurfacePixmapInternal::update()
{
    const InternalWindow *window = m_item->window();
    setBuffer(window->graphicsBuffer());
    setBufferOrigin(window->graphicsBufferOrigin());
}

bool SurfacePixmapInternal::isValid() const
{
    return m_bufferRef;
}

} // namespace KWin

#include "moc_surfaceitem_internal.cpp"

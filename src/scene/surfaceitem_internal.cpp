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

SurfaceItemInternal::SurfaceItemInternal(InternalWindow *window, Item *parent)
    : SurfaceItem(parent)
    , m_window(window)
{
    connect(window, &InternalWindow::presented,
            this, &SurfaceItemInternal::handlePresented);

    setDestinationSize(window->bufferGeometry().size());
    setBufferSourceBox(QRectF(QPointF(0, 0), window->bufferGeometry().size() * window->bufferScale()));
    setBufferSize((window->bufferGeometry().size() * window->bufferScale()).toSize());
    setBufferTransform(m_window->bufferTransform());
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

void SurfaceItemInternal::handlePresented(const InternalWindowFrame &frame)
{
    setDestinationSize(m_window->bufferGeometry().size());
    setBufferSourceBox(QRectF(QPointF(0, 0), frame.buffer->size()));
    setBufferSize(frame.buffer->size());
    setBufferTransform(frame.bufferTransform);

    addDamage(frame.bufferDamage);
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
}

bool SurfacePixmapInternal::isValid() const
{
    return m_bufferRef;
}

} // namespace KWin

#include "moc_surfaceitem_internal.cpp"

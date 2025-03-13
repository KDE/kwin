/*
    SPDX-FileCopyrightText: 2021 Vlad Zahorodnii <vlad.zahorodnii@kde.org>

    SPDX-License-Identifier: GPL-2.0-or-later
*/

#include "scene/surfaceitem_internal.h"
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
    setBuffer(m_window->graphicsBuffer());
    setBufferSourceBox(QRectF(QPointF(0, 0), window->bufferGeometry().size() * window->bufferScale()));
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

void SurfaceItemInternal::handlePresented(const InternalWindowFrame &frame)
{
    setDestinationSize(m_window->bufferGeometry().size());
    setBuffer(frame.buffer);
    setBufferSourceBox(QRectF(QPointF(0, 0), frame.buffer->size()));
    setBufferTransform(frame.bufferTransform);

    addDamage(frame.bufferDamage);
}

} // namespace KWin

#include "moc_surfaceitem_internal.cpp"

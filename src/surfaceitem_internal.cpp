/*
    SPDX-FileCopyrightText: 2021 Vlad Zahorodnii <vlad.zahorodnii@kde.org>

    SPDX-License-Identifier: GPL-2.0-or-later
*/

#include "surfaceitem_internal.h"

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

WindowPixmap *SurfaceItemInternal::createPixmap()
{
    return window()->createWindowPixmap();
}

void SurfaceItemInternal::handleBufferGeometryChanged(Toplevel *toplevel, const QRect &old)
{
    if (toplevel->bufferGeometry().size() != old.size()) {
        discardPixmap();
    }
    setSize(toplevel->bufferGeometry().size());
}

} // namespace KWin

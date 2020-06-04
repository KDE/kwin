/********************************************************************
 KWin - the KDE window manager
 This file is part of the KDE project.

Copyright (C) 2020 Vlad Zahorodnii <vlad.zahorodnii@kde.org>

This program is free software; you can redistribute it and/or modify
it under the terms of the GNU General Public License as published by
the Free Software Foundation; either version 2 of the License, or
(at your option) any later version.

This program is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
GNU General Public License for more details.

You should have received a copy of the GNU General Public License
along with this program.  If not, see <http://www.gnu.org/licenses/>.
*********************************************************************/

#pragma once

#include <QObject>

namespace KWaylandServer
{
class SurfaceInterface;
class SubSurfaceInterface;
}

namespace KWin
{

/**
 * The SubSurfaceMonitor class provides a convenient way for monitoring changes in
 * sub-surface trees, e.g. addition or removal of sub-surfaces, etc.
 */
class SubSurfaceMonitor : public QObject
{
    Q_OBJECT

public:
    /**
     * Constructs a SubSurfaceTreeMonitor with the given @a surface and @a parent.
     */
    SubSurfaceMonitor(KWaylandServer::SurfaceInterface *surface, QObject *parent);

Q_SIGNALS:
    /**
     * This signal is emitted when a new sub-surface has been added to the tree.
     */
    void subSurfaceAdded();
    /**
     * This signal is emitted when a sub-surface has been removed from the tree.
     */
    void subSurfaceRemoved();
    /**
     * This signal is emitted when a sub-surface has been moved relative to its parent.
     */
    void subSurfaceMoved();
    /**
     * This signal is emitted when a sub-surface has been resized.
     */
    void subSurfaceResized();
    /**
     * This signal is emitted when a sub-surface is mapped.
     */
    void subSurfaceMapped();
    /**
     * This signal is emitted when a sub-surface is unmapped.
     */
    void subSurfaceUnmapped();
    /**
     * This signal is emitted when the buffer scale of a sub-surface has been changed.
     */
    void subSurfaceBufferScaleChanged();

private:
    void registerSubSurface(KWaylandServer::SubSurfaceInterface *subSurface);
    void unregisterSubSurface(KWaylandServer::SubSurfaceInterface *subSurface);
    void registerSurface(KWaylandServer::SurfaceInterface *surface);
    void unregisterSurface(KWaylandServer::SurfaceInterface *surface);
};

} // namespace KWin

/*
    KWin - the KDE window manager
    This file is part of the KDE project.

    SPDX-FileCopyrightText: 2020 Vlad Zahorodnii <vlad.zahorodnii@kde.org>

    SPDX-License-Identifier: GPL-2.0-or-later
*/

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
     * This signal is emitted when the mapping between the surface-local coordinate space
     * and the buffer coordinate space for a sub-surface has changed.
     */
    void subSurfaceSurfaceToBufferMatrixChanged();
    /**
     * This signal is emitted when the buffer size of a subsurface has changed.
     */
    void subSurfaceBufferSizeChanged();
    void subSurfaceCommitted(KWaylandServer::SubSurfaceInterface *subSurface);

private:
    void registerSubSurface(KWaylandServer::SubSurfaceInterface *subSurface);
    void unregisterSubSurface(KWaylandServer::SubSurfaceInterface *subSurface);
    void registerSurface(KWaylandServer::SurfaceInterface *surface);
    void unregisterSurface(KWaylandServer::SurfaceInterface *surface);
};

} // namespace KWin

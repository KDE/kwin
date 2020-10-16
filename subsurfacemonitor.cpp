/*
    KWin - the KDE window manager
    This file is part of the KDE project.

    SPDX-FileCopyrightText: 2020 Vlad Zahorodnii <vlad.zahorodnii@kde.org>

    SPDX-License-Identifier: GPL-2.0-or-later
*/

#include "subsurfacemonitor.h"

#include <KWaylandServer/subcompositor_interface.h>
#include <KWaylandServer/surface_interface.h>

using namespace KWaylandServer;

namespace KWin
{

SubSurfaceMonitor::SubSurfaceMonitor(SurfaceInterface *surface, QObject *parent)
    : QObject(parent)
{
    registerSurface(surface);
}

void SubSurfaceMonitor::registerSubSurface(SubSurfaceInterface *subSurface)
{
    SurfaceInterface *surface = subSurface->surface();

    connect(subSurface, &SubSurfaceInterface::positionChanged,
            this, &SubSurfaceMonitor::subSurfaceMoved);
    connect(surface, &SurfaceInterface::sizeChanged,
            this, &SubSurfaceMonitor::subSurfaceResized);
    connect(surface, &SurfaceInterface::mapped,
            this, &SubSurfaceMonitor::subSurfaceMapped);
    connect(surface, &SurfaceInterface::unmapped,
            this, &SubSurfaceMonitor::subSurfaceUnmapped);
    connect(surface, &SurfaceInterface::surfaceToBufferMatrixChanged,
            this, &SubSurfaceMonitor::subSurfaceSurfaceToBufferMatrixChanged);
    connect(surface, &SurfaceInterface::bufferSizeChanged,
            this, &SubSurfaceMonitor::subSurfaceBufferSizeChanged);

    registerSurface(surface);
}

void SubSurfaceMonitor::unregisterSubSurface(SubSurfaceInterface *subSurface)
{
    SurfaceInterface *surface = subSurface->surface();
    if (!surface)
        return;

    disconnect(subSurface, &SubSurfaceInterface::positionChanged,
               this, &SubSurfaceMonitor::subSurfaceMoved);
    disconnect(surface, &SurfaceInterface::sizeChanged,
               this, &SubSurfaceMonitor::subSurfaceResized);
    disconnect(surface, &SurfaceInterface::mapped,
               this, &SubSurfaceMonitor::subSurfaceMapped);
    disconnect(surface, &SurfaceInterface::unmapped,
               this, &SubSurfaceMonitor::subSurfaceUnmapped);
    disconnect(surface, &SurfaceInterface::surfaceToBufferMatrixChanged,
               this, &SubSurfaceMonitor::subSurfaceSurfaceToBufferMatrixChanged);
    disconnect(surface, &SurfaceInterface::bufferSizeChanged,
               this, &SubSurfaceMonitor::subSurfaceBufferSizeChanged);

    unregisterSurface(surface);
}

void SubSurfaceMonitor::registerSurface(SurfaceInterface *surface)
{
    connect(surface, &SurfaceInterface::childSubSurfaceAdded,
            this, &SubSurfaceMonitor::subSurfaceAdded);
    connect(surface, &SurfaceInterface::childSubSurfaceRemoved,
            this, &SubSurfaceMonitor::subSurfaceRemoved);
    connect(surface, &SurfaceInterface::childSubSurfaceAdded,
            this, &SubSurfaceMonitor::registerSubSurface);
    connect(surface, &SurfaceInterface::childSubSurfaceRemoved,
            this, &SubSurfaceMonitor::unregisterSubSurface);
}

void SubSurfaceMonitor::unregisterSurface(SurfaceInterface *surface)
{
    disconnect(surface, &SurfaceInterface::childSubSurfaceAdded,
               this, &SubSurfaceMonitor::subSurfaceAdded);
    disconnect(surface, &SurfaceInterface::childSubSurfaceRemoved,
               this, &SubSurfaceMonitor::subSurfaceRemoved);
    disconnect(surface, &SurfaceInterface::childSubSurfaceAdded,
               this, &SubSurfaceMonitor::registerSubSurface);
    disconnect(surface, &SurfaceInterface::childSubSurfaceRemoved,
               this, &SubSurfaceMonitor::unregisterSubSurface);
}

} // namespace KWin

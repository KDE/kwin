/*
    SPDX-FileCopyrightText: 2014 Martin Gräßlin <mgraesslin@kde.org>
    SPDX-FileCopyrightText: 2020 Vlad Zahorodnii <vlad.zahorodnii@kde.org>

    SPDX-License-Identifier: LGPL-2.1-only OR LGPL-3.0-only OR LicenseRef-KDE-Accepted-LGPL
*/
#ifndef WAYLAND_SERVER_COMPOSITOR_INTERFACE_H
#define WAYLAND_SERVER_COMPOSITOR_INTERFACE_H

#include "region_interface.h"
#include "surface_interface.h"

#include <QObject>

#include <KWaylandServer/kwaylandserver_export.h>

namespace KWaylandServer
{

class CompositorInterfacePrivate;
class Display;

/**
 * The CompositorInterface global allows clients to create surfaces and region objects.
 *
 * The CompositorInterface corresponds to the Wayland interface @c wl_compositor.
 */
class KWAYLANDSERVER_EXPORT CompositorInterface : public QObject
{
    Q_OBJECT

public:
    explicit CompositorInterface(Display *display, QObject *parent = nullptr);
    ~CompositorInterface() override;

    /**
     * Returns the Display object for this CompositorInterface.
     */
    Display *display() const;

Q_SIGNALS:
    /**
     * This signal is emitted when a new SurfaceInterface @a surface has been created.
     */
    void surfaceCreated(KWaylandServer::SurfaceInterface *surface);
    /**
     * This signal is emitted when a new RegionInterface @a region has been created.
     */
    void regionCreated(KWaylandServer::RegionInterface *region);

private:
    QScopedPointer<CompositorInterfacePrivate> d;
};

} // namespace KWaylandServer

#endif

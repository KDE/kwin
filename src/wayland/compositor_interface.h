/*
    SPDX-FileCopyrightText: 2014 Martin Gräßlin <mgraesslin@kde.org>

    SPDX-License-Identifier: LGPL-2.1-only OR LGPL-3.0-only OR LicenseRef-KDE-Accepted-LGPL
*/
#ifndef WAYLAND_SERVER_COMPOSITOR_INTERFACE_H
#define WAYLAND_SERVER_COMPOSITOR_INTERFACE_H

#include "global.h"
#include "region_interface.h"
#include "surface_interface.h"

#include <QObject>

#include <KWayland/Server/kwaylandserver_export.h>

namespace KWayland
{
namespace Server
{

class Display;
class SurfaceInterface;

/**
 * @brief Represents the Global for wl_compositor interface.
 *
 **/
class KWAYLANDSERVER_EXPORT CompositorInterface : public Global
{
    Q_OBJECT
public:
    virtual ~CompositorInterface();

Q_SIGNALS:
    /**
     * Emitted whenever this CompositorInterface created a SurfaceInterface.
     **/
    void surfaceCreated(KWayland::Server::SurfaceInterface*);
    /**
     * Emitted whenever this CompositorInterface created a RegionInterface.
     **/
    void regionCreated(KWayland::Server::RegionInterface*);

private:
    explicit CompositorInterface(Display *display, QObject *parent = nullptr);
    friend class Display;
    class Private;
};

}
}

#endif

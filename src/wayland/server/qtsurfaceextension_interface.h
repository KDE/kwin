/*
    SPDX-FileCopyrightText: 2015 Martin Gräßlin <mgraesslin@kde.org>

    SPDX-License-Identifier: LGPL-2.1-only OR LGPL-3.0-only OR LicenseRef-KDE-Accepted-LGPL
*/
#ifndef WAYLAND_SERVER_QTSURFACEEXTENSION_INTERFACE_H
#define WAYLAND_SERVER_QTSURFACEEXTENSION_INTERFACE_H

#include <QObject>

#include <KWayland/Server/kwaylandserver_export.h>

#include "global.h"
#include "resource.h"

class QSize;
struct wl_resource;

namespace KWayland
{
namespace Server
{

class Display;
class SurfaceInterface;
class QtExtendedSurfaceInterface;

/**
 * TODO
 */
class KWAYLANDSERVER_EXPORT QtSurfaceExtensionInterface : public Global
{
    Q_OBJECT
public:
    virtual ~QtSurfaceExtensionInterface();

Q_SIGNALS:
    void surfaceCreated(KWayland::Server::QtExtendedSurfaceInterface*);

private:
    friend class Display;
    explicit QtSurfaceExtensionInterface(Display *display, QObject *parent);
    class Private;
};

/**
 * TODO
 */
class KWAYLANDSERVER_EXPORT QtExtendedSurfaceInterface : public Resource
{
    Q_OBJECT
public:
    virtual ~QtExtendedSurfaceInterface();

    SurfaceInterface *surface() const;
    QtSurfaceExtensionInterface *shell() const;

    void close();

Q_SIGNALS:
    /**
     * Requests that the window be raised to appear above other windows.
     * @since 5.5
     **/
    void raiseRequested();
    /**
     * Requests that the window be lowered to appear below other windows.
     * @since 5.5
     **/
    void lowerRequested();

private:
    friend class QtSurfaceExtensionInterface;
    explicit QtExtendedSurfaceInterface(QtSurfaceExtensionInterface *shell, SurfaceInterface *parent, wl_resource *parentResource);
    class Private;
    Private *d_func() const;
};

}
}

#endif

/*
    SPDX-FileCopyrightText: 2016 Martin Gräßlin <mgraesslin@kde.org>

    SPDX-License-Identifier: LGPL-2.1-only OR LGPL-3.0-only OR LicenseRef-KDE-Accepted-LGPL
*/
#ifndef KWAYLAND_SERVER_XDGSHELL_INTERFACE_P_H
#define KWAYLAND_SERVER_XDGSHELL_INTERFACE_P_H
#include "xdgshell_interface.h"
#include "global_p.h"
#include "generic_shell_surface_p.h"
#include "resource_p.h"

#include <QTimer>

namespace KWayland
{
namespace Server
{

class XdgShellInterface::Private : public Global::Private
{
public:
    XdgShellInterfaceVersion interfaceVersion;

    virtual quint32 ping(XdgShellSurfaceInterface * surface) = 0;
    void setupTimer(quint32 serial);
    //pingserial/timer correspondence
    QHash <quint32, QTimer *> pingTimers;

protected:
    Private(XdgShellInterfaceVersion interfaceVersion, XdgShellInterface *q, Display *d, const wl_interface *interface, quint32 version);
    XdgShellInterface *q;
};

class XdgShellSurfaceInterface::Private : public Resource::Private, public GenericShellSurface<XdgShellSurfaceInterface>
{
public:
    virtual ~Private();

    virtual void close() = 0;
    virtual quint32 configure(States states, const QSize &size) = 0;
    virtual QRect windowGeometry() const = 0;
    virtual QSize minimumSize() const = 0;
    virtual QSize maximumSize() const = 0;

    XdgShellSurfaceInterface *q_func() {
        return reinterpret_cast<XdgShellSurfaceInterface *>(q);
    }

    QVector<quint32> configureSerials;
    QPointer<XdgShellSurfaceInterface> parent;
    XdgShellInterfaceVersion interfaceVersion;

protected:
    Private(XdgShellInterfaceVersion interfaceVersion, XdgShellSurfaceInterface *q, Global *c, SurfaceInterface *surface, wl_resource *parentResource, const wl_interface *interface, const void *implementation);
};

class XdgShellPopupInterface::Private : public Resource::Private, public GenericShellSurface<XdgShellPopupInterface>
{
public:
    virtual ~Private();
    virtual void popupDone() = 0;
    virtual QRect windowGeometry() const = 0;

    XdgShellPopupInterface *q_func() {
        return reinterpret_cast<XdgShellPopupInterface *>(q);
    }

    virtual quint32 configure(const QRect &rect) {
        Q_UNUSED(rect)
        return 0;
    };

    QVector<quint32> configureSerials;
    QPointer<SurfaceInterface> parent;
    QSize initialSize;

    /*
     *
     */
    QRect anchorRect;
    Qt::Edges anchorEdge;
    Qt::Edges gravity;
    PositionerConstraints constraintAdjustments;
    QPoint anchorOffset;

    XdgShellInterfaceVersion interfaceVersion;

protected:
    Private(XdgShellInterfaceVersion interfaceVersion, XdgShellPopupInterface *q, XdgShellInterface *c, SurfaceInterface *surface, wl_resource *parentResource, const wl_interface *interface, const void *implementation);

};

}
}

#endif

/*
    SPDX-FileCopyrightText: 2017 Martin Flöser <mgraesslin@kde.org>

    SPDX-License-Identifier: LGPL-2.1-only OR LGPL-3.0-only OR LicenseRef-KDE-Accepted-LGPL
*/
#ifndef KWAYLAND_SERVER_IDLEINHIBIT_INTERFACE_H
#define KWAYLAND_SERVER_IDLEINHIBIT_INTERFACE_H

#include <QObject>

#include <KWaylandServer/kwaylandserver_export.h>

struct wl_resource;

namespace KWaylandServer
{

class Display;
class IdleInhibitManagerV1InterfacePrivate;

/**
 * The IdleInhibitorManagerInterface is used by clients to inhibit idle on a
 * SurfaceInterface. Whether a SurfaceInterface inhibits idle is exposes through
 * @link{SurfaceInterface::inhibitsIdle}.
 *
 * @since 5.41
 **/
class KWAYLANDSERVER_EXPORT IdleInhibitManagerV1Interface : public QObject
{
    Q_OBJECT

public:
    explicit IdleInhibitManagerV1Interface(Display *display, QObject *parent = nullptr);
    ~IdleInhibitManagerV1Interface() override;

private:
    QScopedPointer<IdleInhibitManagerV1InterfacePrivate> d;
};

}

#endif

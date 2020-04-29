/*
    SPDX-FileCopyrightText: 2015 Martin Gräßlin <mgraesslin@kde.org>

    SPDX-License-Identifier: LGPL-2.1-only OR LGPL-3.0-only OR LicenseRef-KDE-Accepted-LGPL
*/
#ifndef WAYLAND_SERVER_TOUCH_INTERFACE_H
#define WAYLAND_SERVER_TOUCH_INTERFACE_H

#include <KWaylandServer/kwaylandserver_export.h>

#include "resource.h"

namespace KWaylandServer
{

class SeatInterface;

/**
 * @brief Resource for the wl_touch interface.
 *
 **/
class KWAYLANDSERVER_EXPORT TouchInterface : public Resource
{
    Q_OBJECT
public:
    virtual ~TouchInterface();

private:
    void down(qint32 id, quint32 serial, const QPointF &localPos);
    void up(qint32 id, quint32 serial);
    void frame();
    void cancel();
    void move(qint32 id, const QPointF &localPos);
    friend class SeatInterface;
    explicit TouchInterface(SeatInterface *parent, wl_resource *parentResource);
    class Private;
    Private *d_func() const;
};

}

Q_DECLARE_METATYPE(KWaylandServer::TouchInterface*)

#endif

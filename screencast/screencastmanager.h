/*
 * Copyright © 2020 Aleix Pol Gonzalez <aleixpol@kde.org>
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with this library. If not, see <http://www.gnu.org/licenses/>.
 *
 * Authors:
 *       Jan Grulich <jgrulich@redhat.com>
 *       Aleix Pol Gonzalez <aleixpol@kde.org>
 */

#pragma once

#include "wayland_server.h"
#include <KWaylandServer/screencast_interface.h>

class PipeWireStream;

namespace KWin
{
class AbstractEglBackend;
}

class ScreencastManager
    : public QObject
{
    Q_OBJECT
public:
    ScreencastManager(QObject *parent);

    void streamWindow(KWaylandServer::ScreencastStreamInterface *stream, const QString &winid);
    void streamOutput(KWaylandServer::ScreencastStreamInterface *stream,
                                  ::wl_resource *outputResource,
                                  KWaylandServer::ScreencastInterface::CursorMode mode);

private:
    void integrateStreams(KWaylandServer::ScreencastStreamInterface *waylandStream, PipeWireStream *pipewireStream);
};


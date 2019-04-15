/****************************************************************************
Copyright 2019 NVIDIA Inc.

This library is free software; you can redistribute it and/or
modify it under the terms of the GNU Lesser General Public
License as published by the Free Software Foundation; either
version 2.1 of the License, or (at your option) version 3, or any
later version accepted by the membership of KDE e.V. (or its
successor approved by the membership of KDE e.V.), which shall
act as a proxy defined in Section 6 of version 3 of the license.

This library is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
Lesser General Public License for more details.

You should have received a copy of the GNU Lesser General Public
License along with this library.  If not, see <http://www.gnu.org/licenses/>.
****************************************************************************/
#ifndef WAYLAND_SERVER_EGLSTREAM_CONTROLLER_INTERFACE_H
#define WAYLAND_SERVER_EGLSTREAM_CONTROLLER_INTERFACE_H

#include "global.h"
#include "surface_interface.h"

#include <KWayland/Server/kwaylandserver_export.h>
#include <wayland-util.h>
#include <QObject>

namespace KWayland
{
namespace Server
{

class Display;

/**
 * @brief Represents the Global for the wl_eglstream_controller interface.
 *
 * This class handles requests (typically from the NVIDIA EGL driver) to attach
 * a newly created EGL Stream to a Wayland surface, facilitating the sharing
 * of buffer contents between client and compositor.
 *
 */
class KWAYLANDSERVER_EXPORT EglStreamControllerInterface : public Global
{
    Q_OBJECT
public:
    ~EglStreamControllerInterface() override;
    void create();

Q_SIGNALS:
    /**
     * Emitted when a new stream attach request is received.
     */
    void streamConsumerAttached(SurfaceInterface *surface, void *eglStream, wl_array *attribs);
private:
    explicit EglStreamControllerInterface(Display *display, QObject *parent = nullptr);
    
    class Private;
    friend class Display;
};

}
}

#endif

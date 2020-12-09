/*
    SPDX-FileCopyrightText: 2019 NVIDIA Inc.

    SPDX-License-Identifier: LGPL-2.1-only OR LGPL-3.0-only OR LicenseRef-KDE-Accepted-LGPL
*/
#ifndef WAYLAND_SERVER_EGLSTREAM_CONTROLLER_INTERFACE_H
#define WAYLAND_SERVER_EGLSTREAM_CONTROLLER_INTERFACE_H

#include "global.h"
#include "surface_interface.h"

#include <KWaylandServer/kwaylandserver_export.h>
#include <wayland-util.h>
#include <QObject>

namespace KWaylandServer
{

class Display;
class EglStreamControllerInterfacePrivate;
/**
 * @brief Represents the Global for the wl_eglstream_controller interface.
 *
 * This class handles requests (typically from the NVIDIA EGL driver) to attach
 * a newly created EGL Stream to a Wayland surface, facilitating the sharing
 * of buffer contents between client and compositor.
 *
 */
class KWAYLANDSERVER_EXPORT EglStreamControllerInterface : public QObject
{
    Q_OBJECT

public:
    explicit EglStreamControllerInterface(Display *display, QObject *parent = nullptr);
    ~EglStreamControllerInterface() override;

Q_SIGNALS:
    /**
     * Emitted when a new stream attach request is received.
     */
    void streamConsumerAttached(SurfaceInterface *surface, void *eglStream, wl_array *attribs);

private:
    QScopedPointer<EglStreamControllerInterfacePrivate> d;
};

}

#endif

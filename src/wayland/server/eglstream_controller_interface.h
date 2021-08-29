/*
    SPDX-FileCopyrightText: 2019 NVIDIA Inc.

    SPDX-License-Identifier: LGPL-2.1-only OR LGPL-3.0-only OR LicenseRef-KDE-Accepted-LGPL
*/
#pragma once

#include "surface_interface.h"

#include <KWaylandServer/kwaylandserver_export.h>
#include <QObject>
#include <wayland-util.h>

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

/*
    SPDX-FileCopyrightText: 2019 NVIDIA Inc.

    SPDX-License-Identifier: LGPL-2.1-only OR LGPL-3.0-only OR LicenseRef-KDE-Accepted-LGPL
*/
#ifndef WAYLAND_SERVER_EGLSTREAM_CONTROLLER_INTERFACE_P_H
#define WAYLAND_SERVER_EGLSTREAM_CONTROLLER_INTERFACE_P_H

#include "eglstream_controller_interface.h"
#include "global_p.h"

#include <wayland-eglstream-controller-server-protocol.h>

namespace KWayland
{
namespace Server
{

class Q_DECL_HIDDEN EglStreamControllerInterface::Private : public Global::Private
{
public:
    Private(EglStreamControllerInterface *controller, Display *display);
    void create();

private:
    static void attachStreamConsumer(wl_client *client,
                                     wl_resource *resource,
                                     wl_resource *surface,
                                     wl_resource *eglStream);
    static void attachStreamConsumerAttribs(wl_client *client,
                                            wl_resource *resource,
                                            wl_resource *surface,
                                            wl_resource *eglStream,
                                            wl_array *attribs);
    static const struct wl_eglstream_controller_interface s_interface;
    static const quint32 s_version;
    void bind(wl_client *client, uint32_t version, uint32_t id) override;
    EglStreamControllerInterface *q;
};

}
}

#endif

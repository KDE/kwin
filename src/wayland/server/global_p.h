/*
    SPDX-FileCopyrightText: 2014 Martin Gräßlin <mgraesslin@kde.org>

    SPDX-License-Identifier: LGPL-2.1-only OR LGPL-3.0-only OR LicenseRef-KDE-Accepted-LGPL
*/
#ifndef WAYLAND_SERVER_GLOBAL_P_H
#define WAYLAND_SERVER_GLOBAL_P_H

#include "global.h"

#include <QPointer>

#include <wayland-server-core.h>

namespace KWaylandServer
{

struct DisplayDestroyListener : public wl_listener
{
    Global *global = nullptr;
};

class Global::Private
{
public:
    static constexpr quint32 version = 0;
    virtual ~Private();
    void create();

    // We need to reset display from the destroy listener, but due to the private class
    // being nested, this is not easy to do so. Either way, we are moving away from the
    // old approach, so it's not worth wasting our time.
    QPointer<Display> display;
    wl_global *global = nullptr;
    DisplayDestroyListener displayDestroyListener;

protected:
    Private(Display *d, const wl_interface *interface, quint32 version);
    virtual void bind(wl_client *client, uint32_t version, uint32_t id) = 0;

    static void bind(wl_client *client, void *data, uint32_t version, uint32_t id);

    const wl_interface *const m_interface;
    const quint32 m_version;
};

}

#endif

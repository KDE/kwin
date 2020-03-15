/*
    SPDX-FileCopyrightText: 2014 Martin Gräßlin <mgraesslin@kde.org>

    SPDX-License-Identifier: LGPL-2.1-only OR LGPL-3.0-only OR LicenseRef-KDE-Accepted-LGPL
*/
#ifndef WAYLAND_SERVER_GLOBAL_P_H
#define WAYLAND_SERVER_GLOBAL_P_H

#include "global.h"

struct wl_client;
struct wl_interface;

namespace KWayland
{
namespace Server
{

class Global::Private
{
public:
    static constexpr quint32 version = 0;
    virtual ~Private();
    void create();

    Display *display = nullptr;
    wl_global *global = nullptr;

protected:
    Private(Display *d, const wl_interface *interface, quint32 version);
    virtual void bind(wl_client *client, uint32_t version, uint32_t id) = 0;

    static void bind(wl_client *client, void *data, uint32_t version, uint32_t id);

    const wl_interface *const m_interface;
    const quint32 m_version;
};

}
}

#endif

/*
    SPDX-FileCopyrightText: 2014 Martin Gräßlin <mgraesslin@kde.org>

    SPDX-License-Identifier: LGPL-2.1-only OR LGPL-3.0-only OR LicenseRef-KDE-Accepted-LGPL
*/
#include "global.h"
#include "global_p.h"
#include "display.h"
// wayland
#include <wayland-server.h>

namespace KWaylandServer
{

Global::Private::Private(Display *d, const wl_interface *interface, quint32 version)
    : display(d)
    , m_interface(interface)
    , m_version(version)
{
}

Global::Private::~Private() = default;

void Global::Private::bind(wl_client *client, void *data, uint32_t version, uint32_t id)
{
    auto d = reinterpret_cast<Private*>(data);
    if (!d) {
        return;
    }
    d->bind(client, version, id);
}

void Global::Private::create()
{
    global = wl_global_create(*display, m_interface, m_version, this, bind);
}

Global::Global(Global::Private *d, QObject *parent)
    : QObject(parent)
    , d(d)
{
}

Global::~Global()
{
    destroy();
}

void Global::create()
{
    d->create();
}

void Global::remove()
{
    if (!d->global) {
        return;
    }
    wl_global_remove(d->global);
}

void Global::destroy()
{
    if (!d->global) {
        return;
    }
    emit aboutToDestroyGlobal();
    wl_global_destroy(d->global);
    d->global = nullptr;
}

bool Global::isValid() const
{
    return d->global != nullptr;
}

Global::operator wl_global*() const
{
    return d->global;
}

Global::operator wl_global*()
{
    return d->global;
}

Display *Global::display()
{
    return d->display;
}

}

/*
    SPDX-FileCopyrightText: 2025 Xaver Hugl <xaver.hugl@gmail.com>

    SPDX-License-Identifier: LGPL-2.1-only OR LGPL-3.0-only OR LicenseRef-KDE-Accepted-LGPL
*/
#include "xdgtopleveltag_v1.h"
#include "display.h"
#include "xdgshell_p.h"

namespace KWin
{

static constexpr uint32_t s_version = 1;

XdgToplevelTagManagerV1::XdgToplevelTagManagerV1(Display *display, QObject *parent)
    : QObject(parent)
    , QtWaylandServer::xdg_toplevel_tag_manager_v1(*display, s_version)
{
}

void XdgToplevelTagManagerV1::xdg_toplevel_tag_manager_v1_destroy(Resource *resource)
{
    wl_resource_destroy(resource->handle);
}

void XdgToplevelTagManagerV1::xdg_toplevel_tag_manager_v1_set_toplevel_tag(Resource *resource, ::wl_resource *toplevel, const QString &tag)
{
    auto interface = XdgToplevelInterfacePrivate::get(toplevel);
    interface->tag = tag;
    Q_EMIT interface->q->tagChanged();
}

void XdgToplevelTagManagerV1::xdg_toplevel_tag_manager_v1_set_toplevel_description(Resource *resource, ::wl_resource *toplevel, const QString &description)
{
    auto interface = XdgToplevelInterfacePrivate::get(toplevel);
    interface->description = description;
    Q_EMIT interface->q->descriptionChanged();
}
}

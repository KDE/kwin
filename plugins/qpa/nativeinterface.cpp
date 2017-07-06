/********************************************************************
 KWin - the KDE window manager
 This file is part of the KDE project.

Copyright (C) 2015 Martin Gräßlin <mgraesslin@kde.org>

This program is free software; you can redistribute it and/or modify
it under the terms of the GNU General Public License as published by
the Free Software Foundation; either version 2 of the License, or
(at your option) any later version.

This program is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
GNU General Public License for more details.

You should have received a copy of the GNU General Public License
along with this program.  If not, see <http://www.gnu.org/licenses/>.
*********************************************************************/
#include "nativeinterface.h"
#include "integration.h"
#include "window.h"
#include "../../wayland_server.h"

#include <QWindow>

#include <KWayland/Client/connection_thread.h>
#include <KWayland/Client/compositor.h>
#include <KWayland/Client/surface.h>

namespace KWin
{
namespace QPA
{

static const QByteArray s_displayKey = QByteArrayLiteral("display");
static const QByteArray s_wlDisplayKey = QByteArrayLiteral("wl_display");
static const QByteArray s_compositorKey = QByteArrayLiteral("compositor");
static const QByteArray s_surfaceKey = QByteArrayLiteral("surface");

NativeInterface::NativeInterface(Integration *integration)
    : QPlatformNativeInterface()
    , m_integration(integration)
{
}

void *NativeInterface::nativeResourceForIntegration(const QByteArray &resource)
{
    const QByteArray r = resource.toLower();
    if (r == s_displayKey || r == s_wlDisplayKey) {
        if (!waylandServer() || !waylandServer()->internalClientConection()) {
            return nullptr;
        }
        return waylandServer()->internalClientConection()->display();
    }
    if (r == s_compositorKey) {
        return static_cast<wl_compositor*>(*m_integration->compositor());
    }
    return nullptr;
}

void *NativeInterface::nativeResourceForWindow(const QByteArray &resource, QWindow *window)
{
    const QByteArray r = resource.toLower();
    if (r == s_displayKey || r == s_wlDisplayKey) {
        if (!waylandServer() || !waylandServer()->internalClientConection()) {
            return nullptr;
        }
        return waylandServer()->internalClientConection()->display();
    }
    if (r == s_compositorKey) {
        return static_cast<wl_compositor*>(*m_integration->compositor());
    }
    if (r == s_surfaceKey && window) {
        if (auto handle = window->handle()) {
            if (auto surface = static_cast<Window*>(handle)->surface()) {
                return static_cast<wl_surface*>(*surface);
            }
        }
    }
    return nullptr;
}

static void roundtrip()
{
    if (!waylandServer()) {
        return;
    }
    auto c = waylandServer()->internalClientConection();
    if (!c) {
        return;
    }
    c->flush();
    waylandServer()->dispatch();
}

QFunctionPointer NativeInterface::platformFunction(const QByteArray &function) const
{
    if (qstrcmp(function.toLower(), "roundtrip") == 0) {
        return &roundtrip;
    }
    return nullptr;
}

}
}

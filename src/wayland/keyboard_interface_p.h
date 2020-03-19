/*
    SPDX-FileCopyrightText: 2016 Martin Gräßlin <mgraesslin@kde.org>

    SPDX-License-Identifier: LGPL-2.1-only OR LGPL-3.0-only OR LicenseRef-KDE-Accepted-LGPL
*/
#ifndef WAYLAND_SERVER_KEYBOARD_INTERFACE_P_H
#define WAYLAND_SERVER_KEYBOARD_INTERFACE_P_H
#include "keyboard_interface.h"
#include "resource_p.h"

#include <QPointer>

class QTemporaryFile;

namespace KWayland
{
namespace Server
{

class KeyboardInterface::Private : public Resource::Private
{
public:
    Private(SeatInterface *s, wl_resource *parentResource, KeyboardInterface *q);
    void sendKeymap(int fd, quint32 size);
    void sendModifiers();
    void sendModifiers(quint32 depressed, quint32 latched, quint32 locked, quint32 group, quint32 serial);

    void focusChildSurface(const QPointer<SurfaceInterface> &childSurface, quint32 serial);
    void sendLeave(SurfaceInterface *surface, quint32 serial);
    void sendEnter(SurfaceInterface *surface, quint32 serial);

    SeatInterface *seat;
    SurfaceInterface *focusedSurface = nullptr;
    QPointer<SurfaceInterface> focusedChildSurface;
    QMetaObject::Connection destroyConnection;
    QScopedPointer<QTemporaryFile> keymap;

private:
    static const struct wl_keyboard_interface s_interface;
};

}
}

#endif

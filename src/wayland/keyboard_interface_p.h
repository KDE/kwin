/*
    SPDX-FileCopyrightText: 2016 Martin Gräßlin <mgraesslin@kde.org>

    SPDX-License-Identifier: LGPL-2.1-only OR LGPL-3.0-only OR LicenseRef-KDE-Accepted-LGPL
*/
#ifndef WAYLAND_SERVER_KEYBOARD_INTERFACE_P_H
#define WAYLAND_SERVER_KEYBOARD_INTERFACE_P_H
#include "keyboard_interface.h"

#include <qwayland-server-wayland.h>

#include <QPointer>
#include <QHash>

class QTemporaryFile;

namespace KWaylandServer
{

class ClientConnection;

class KeyboardInterfacePrivate : public QtWaylandServer::wl_keyboard
{
public:
    KeyboardInterfacePrivate(SeatInterface *s);


    void sendKeymap(int fd, quint32 size);
    void sendModifiers();
    void sendModifiers(quint32 depressed, quint32 latched, quint32 locked, quint32 group, quint32 serial);

    QList<Resource *> keyboardsForClient(ClientConnection *client) const;
    void focusChildSurface(SurfaceInterface *childSurface, quint32 serial);
    void sendLeave(SurfaceInterface *surface, quint32 serial);
    void sendEnter(SurfaceInterface *surface, quint32 serial);

    static KeyboardInterfacePrivate *get(KeyboardInterface *keyboard) { return keyboard->d.data(); }

    SeatInterface *seat;
    SurfaceInterface *focusedSurface = nullptr;
    QPointer<SurfaceInterface> focusedChildSurface;
    QMetaObject::Connection destroyConnection;
    QScopedPointer<QTemporaryFile> keymap;

    struct {
        qint32 charactersPerSecond = 0;
        qint32 delay = 0;
    } keyRepeat;

    struct Modifiers {
        quint32 depressed = 0;
        quint32 latched = 0;
        quint32 locked = 0;
        quint32 group = 0;
        quint32 serial = 0;
    };
    Modifiers modifiers;

    enum class State {
        Released,
        Pressed
    };
    QHash<quint32, State> states;
    bool updateKey(quint32 key, State state);
    QVector<quint32> pressedKeys() const;

protected:
    void keyboard_bind_resource(Resource *resource) override;
};

}

#endif

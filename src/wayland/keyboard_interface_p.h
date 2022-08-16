/*
    SPDX-FileCopyrightText: 2016 Martin Gräßlin <mgraesslin@kde.org>

    SPDX-License-Identifier: LGPL-2.1-only OR LGPL-3.0-only OR LicenseRef-KDE-Accepted-LGPL
*/
#pragma once

#include "keyboard_interface.h"
#include "utils/ramfile.h"

#include <qwayland-server-wayland.h>

#include <QHash>
#include <QPointer>

namespace KWaylandServer
{
class ClientConnection;

class KeyboardInterfacePrivate : public QtWaylandServer::wl_keyboard
{
public:
    KeyboardInterfacePrivate(SeatInterface *s);

    void sendKeymap(Resource *resource);
    void sendModifiers();
    void sendModifiers(quint32 depressed, quint32 latched, quint32 locked, quint32 group, quint32 serial);

    QList<Resource *> keyboardsForClient(ClientConnection *client) const;
    void sendLeave(SurfaceInterface *surface, quint32 serial);
    void sendEnter(SurfaceInterface *surface, quint32 serial);

    static KeyboardInterfacePrivate *get(KeyboardInterface *keyboard)
    {
        return keyboard->d.get();
    }

    SeatInterface *seat;
    SurfaceInterface *focusedSurface = nullptr;
    QMetaObject::Connection destroyConnection;
    QByteArray keymap;
    KWin::RamFile sharedKeymapFile;

    struct
    {
        qint32 charactersPerSecond = 0;
        qint32 delay = 0;
    } keyRepeat;

    struct Modifiers
    {
        quint32 depressed = 0;
        quint32 latched = 0;
        quint32 locked = 0;
        quint32 group = 0;
        quint32 serial = 0;
    };
    Modifiers modifiers;

    QHash<quint32, KeyboardKeyState> states;
    bool updateKey(quint32 key, KeyboardKeyState state);
    QVector<quint32> pressedKeys() const;

protected:
    void keyboard_release(Resource *resource) override;
    void keyboard_bind_resource(Resource *resource) override;
};

}

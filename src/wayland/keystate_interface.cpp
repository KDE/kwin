/*
    SPDX-FileCopyrightText: 2019 Aleix Pol Gonzalez <aleixpol@kde.org>

    SPDX-License-Identifier: LGPL-2.1-only OR LGPL-3.0-only OR LicenseRef-KDE-Accepted-LGPL
*/

#include "keystate_interface.h"
#include "display.h"

#include "keyboard_input.h"
#include "xkb.h"

#include <QDebug>
#include <QVector>
#include <qwayland-server-keystate.h>

using namespace KWin;

namespace KWaylandServer
{

static const quint32 s_version = 1;

class KeyStateInterfacePrivate : public QtWaylandServer::org_kde_kwin_keystate
{
public:
    KeyStateInterfacePrivate(Display *d)
        : QtWaylandServer::org_kde_kwin_keystate(*d, s_version)
    {
    }

    void org_kde_kwin_keystate_fetchStates(Resource *resource) override
    {
        const LEDs leds = input()->keyboard()->xkb()->leds();

        send_stateChanged(resource->handle, key_capslock, leds & LED::CapsLock ? state_locked : state_unlocked);
        send_stateChanged(resource->handle, key_numlock, leds & LED::NumLock ? state_locked : state_unlocked);
        send_stateChanged(resource->handle, key_scrolllock, leds & LED::ScrollLock ? state_locked : state_unlocked);
    }
};

KeyStateInterface::KeyStateInterface(Display *display, QObject *parent)
    : QObject(parent)
    , d(new KeyStateInterfacePrivate(display))
{
    connect(input()->keyboard(), &KeyboardInputRedirection::ledsChanged, this, [this]() {
        const auto resources = d->resourceMap();
        for (const auto &resource : resources) {
            d->org_kde_kwin_keystate_fetchStates(resource);
        }
    });
}

KeyStateInterface::~KeyStateInterface() = default;

}

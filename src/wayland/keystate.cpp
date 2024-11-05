/*
    SPDX-FileCopyrightText: 2019 Aleix Pol Gonzalez <aleixpol@kde.org>

    SPDX-License-Identifier: LGPL-2.1-only OR LGPL-3.0-only OR LicenseRef-KDE-Accepted-LGPL
*/

#include "keystate.h"
#include "display.h"

#include "keyboard_input.h"
#include "xkb.h"

#include <QDebug>
#include <QList>
#include <qwayland-server-keystate.h>

namespace KWin
{

static const quint32 s_version = 5;

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

        // Scroll is a virtual modifier and xkbcommon doesn't (yet) support querying those
        // See https://github.com/xkbcommon/libxkbcommon/pull/512
        send_stateChanged(resource->handle, key_scrolllock, leds & LED::ScrollLock ? state_locked : state_unlocked);

        auto sendModifier = [this, resource](key k, Xkb::Modifier mod) {
            if (input()->keyboard()->xkb()->lockedModifiers().testFlag(mod)) {
                send_stateChanged(resource->handle, k, state_locked);
            } else if (input()->keyboard()->xkb()->latchedModifiers().testFlag(mod)) {
                send_stateChanged(resource->handle, k, state_latched);
            } else if (input()->keyboard()->xkb()->depressedModifiers().testFlag(mod) && resource->version() >= ORG_KDE_KWIN_KEYSTATE_STATE_PRESSED_SINCE_VERSION) {
                send_stateChanged(resource->handle, k, state_pressed);
            } else {
                send_stateChanged(resource->handle, k, state_unlocked);
            }
        };

        static constexpr int modifierSinceVersion = ORG_KDE_KWIN_KEYSTATE_KEY_ALT_SINCE_VERSION;

        if (resource->version() >= modifierSinceVersion) {
            sendModifier(key_alt, Xkb::Mod1);
            sendModifier(key_shift, Xkb::Shift);
            sendModifier(key_control, Xkb::Control);
            sendModifier(key_meta, Xkb::Mod4);
            sendModifier(key_altgr, Xkb::Mod5);
        }

        sendModifier(key_capslock, Xkb::Lock);
        sendModifier(key_numlock, Xkb::Num);
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

    connect(input()->keyboard()->xkb(), &Xkb::modifierStateChanged, this, [this]() {
        const auto resources = d->resourceMap();
        for (const auto &resource : resources) {
            d->org_kde_kwin_keystate_fetchStates(resource);
        }
    });
}

KeyStateInterface::~KeyStateInterface() = default;

}

#include "moc_keystate.cpp"

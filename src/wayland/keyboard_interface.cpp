/********************************************************************
Copyright 2014  Martin Gräßlin <mgraesslin@kde.org>

This library is free software; you can redistribute it and/or
modify it under the terms of the GNU Lesser General Public
License as published by the Free Software Foundation; either
version 2.1 of the License, or (at your option) version 3, or any
later version accepted by the membership of KDE e.V. (or its
successor approved by the membership of KDE e.V.), which shall
act as a proxy defined in Section 6 of version 3 of the license.

This library is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
Lesser General Public License for more details.

You should have received a copy of the GNU Lesser General Public
License along with this library.  If not, see <http://www.gnu.org/licenses/>.
*********************************************************************/
#include "keyboard_interface.h"
#include "display.h"
#include "seat_interface.h"
#include "surface_interface.h"
// Qt
#include <QHash>
// System
#include <fcntl.h>
#include <unistd.h>
// Wayland
#include <wayland-server.h>

namespace KWayland
{

namespace Server
{

class KeyboardInterface::Private
{
public:
    Private(SeatInterface *s);
    void createInterfae(wl_client *client, wl_resource *parentResource, uint32_t id);
    void surfaceDeleted();
    wl_resource *keyboardForSurface(SurfaceInterface *surface) const;
    void sendKeymap(wl_resource *r);
    void sendKeymapToAll();
    void sendModifiers(wl_resource *r);
    enum class KeyState {
        Released,
        Pressed
    };
    void updateKey(quint32 key, KeyState state);

    SeatInterface *seat;
    struct ResourceData {
        wl_client *client = nullptr;
        wl_resource *keyboard = nullptr;
    };
    QList<ResourceData> resources;
    struct Keymap {
        int fd = -1;
        quint32 size = 0;
        bool xkbcommonCompatible = false;
    };
    Keymap keymap;
    struct Modifiers {
        quint32 depressed = 0;
        quint32 latched = 0;
        quint32 locked = 0;
        quint32 group = 0;
    };
    Modifiers modifiers;
    struct FocusedSurface {
        SurfaceInterface *surface = nullptr;
        wl_resource *keyboard = nullptr;
    };
    FocusedSurface focusedSurface;
    QHash<quint32, KeyState> keyStates;
    quint32 eventTime = 0;
    QMetaObject::Connection destroyConnection;

private:
    static Private *cast(wl_resource *resource) {
        return reinterpret_cast<KeyboardInterface::Private*>(wl_resource_get_user_data(resource));
    }

    static void unbind(wl_resource *resource);
    // since version 3
    static void releaseCallback(wl_client *client, wl_resource *resource);

    static const struct wl_keyboard_interface s_interface;
};

KeyboardInterface::Private::Private(SeatInterface *s)
    : seat(s)
{
}

const struct wl_keyboard_interface KeyboardInterface::Private::s_interface {
    releaseCallback
};

KeyboardInterface::KeyboardInterface(SeatInterface *parent)
    : QObject(parent)
    , d(new Private(parent))
{
}

KeyboardInterface::~KeyboardInterface()
{
    while (!d->resources.isEmpty()) {
        auto data = d->resources.takeLast();
        wl_resource_destroy(data.keyboard);
    }
}

void KeyboardInterface::createInterfae(wl_client *client, wl_resource *parentResource, uint32_t id)
{
    d->createInterfae(client, parentResource, id);
}

void KeyboardInterface::Private::createInterfae(wl_client *client, wl_resource *parentResource, uint32_t id)
{
    wl_resource *k = wl_resource_create(client, &wl_keyboard_interface, wl_resource_get_version(parentResource), id);
    if (!k) {
        wl_resource_post_no_memory(parentResource);
        return;
    }
    ResourceData data;
    data.client = client;
    data.keyboard = k;
    resources << data;

    wl_resource_set_implementation(k, &s_interface, this, unbind);

    sendKeymap(k);
}

void KeyboardInterface::Private::unbind(wl_resource *resource)
{
    auto k = cast(resource);
    auto it = std::find_if(k->resources.begin(), k->resources.end(),
        [resource](const ResourceData &data) {
            return data.keyboard == resource;
        }
    );
    if (it == k->resources.end()) {
        return;
    }
    if ((*it).keyboard == k->focusedSurface.keyboard) {
        QObject::disconnect(k->destroyConnection);
        k->focusedSurface = FocusedSurface();
    }
    k->resources.erase(it);
}

void KeyboardInterface::Private::surfaceDeleted()
{
    focusedSurface = FocusedSurface();
}

void KeyboardInterface::Private::releaseCallback(wl_client *client, wl_resource *resource)
{
    Q_UNUSED(client)
    unbind(resource);
}

void KeyboardInterface::setKeymap(int fd, quint32 size)
{
    d->keymap.xkbcommonCompatible = true;
    d->keymap.fd = fd;
    d->keymap.size = size;
    d->sendKeymapToAll();
}

void KeyboardInterface::Private::sendKeymap(wl_resource *r)
{
    if (keymap.xkbcommonCompatible) {
        wl_keyboard_send_keymap(r,
                                WL_KEYBOARD_KEYMAP_FORMAT_XKB_V1,
                                keymap.fd,
                                keymap.size);
    } else {
        int nullFd = open("/dev/null", O_RDONLY);
        wl_keyboard_send_keymap(r, WL_KEYBOARD_KEYMAP_FORMAT_NO_KEYMAP, nullFd, 0);
        close(nullFd);
    }
}

void KeyboardInterface::Private::sendKeymapToAll()
{
    for (auto it = resources.constBegin(); it != resources.constEnd(); ++it) {
        sendKeymap((*it).keyboard);
    }
}

void KeyboardInterface::Private::sendModifiers(wl_resource* r)
{
    wl_keyboard_send_modifiers(r, seat->display()->nextSerial(), modifiers.depressed, modifiers.latched, modifiers.locked, modifiers.group);
}

void KeyboardInterface::setFocusedSurface(SurfaceInterface *surface)
{
    const quint32 serial = d->seat->display()->nextSerial();
    if (d->focusedSurface.surface && d->focusedSurface.keyboard) {
        wl_keyboard_send_leave(d->focusedSurface.keyboard, serial, d->focusedSurface.surface->resource());
        disconnect(d->destroyConnection);
    }
    d->focusedSurface.keyboard = d->keyboardForSurface(surface);
    if (!d->focusedSurface.keyboard) {
        d->focusedSurface = Private::FocusedSurface();
        return;
    }
    d->focusedSurface.surface = surface;
    d->destroyConnection = connect(d->focusedSurface.surface, &QObject::destroyed, this, [this] { d->surfaceDeleted(); });

    wl_array keys;
    wl_array_init(&keys);
    for (auto it = d->keyStates.constBegin(); it != d->keyStates.constEnd(); ++it) {
        if (it.value() == Private::KeyState::Pressed) {
            continue;
        }
        uint32_t *k = reinterpret_cast<uint32_t*>(wl_array_add(&keys, sizeof(uint32_t)));
        *k = it.key();
    }
    wl_keyboard_send_enter(d->focusedSurface.keyboard, serial, d->focusedSurface.surface->resource(), &keys);
    wl_array_release(&keys);

    d->sendModifiers(d->focusedSurface.keyboard);
}

wl_resource *KeyboardInterface::Private::keyboardForSurface(SurfaceInterface *surface) const
{
    if (!surface) {
        return nullptr;
    }
    for (auto it = resources.constBegin(); it != resources.constEnd(); ++it) {
        if ((*it).client == *surface->client()) {
            return (*it).keyboard;
        }
    }
    return nullptr;
}

void KeyboardInterface::keyPressed(quint32 key)
{
    d->updateKey(key, Private::KeyState::Pressed);
    if (d->focusedSurface.surface && d->focusedSurface.keyboard) {
        wl_keyboard_send_key(d->focusedSurface.keyboard, d->seat->display()->nextSerial(), d->eventTime, key, WL_KEYBOARD_KEY_STATE_PRESSED);
    }
}

void KeyboardInterface::keyReleased(quint32 key)
{
    d->updateKey(key, Private::KeyState::Released);
    if (d->focusedSurface.surface && d->focusedSurface.keyboard) {
        wl_keyboard_send_key(d->focusedSurface.keyboard, d->seat->display()->nextSerial(), d->eventTime, key, WL_KEYBOARD_KEY_STATE_RELEASED);
    }
}

void KeyboardInterface::Private::updateKey(quint32 key, KeyState state)
{
    auto it = keyStates.find(key);
    if (it == keyStates.end()) {
        keyStates.insert(key, state);
        return;
    }
    it.value() = state;
}

void KeyboardInterface::updateTimestamp(quint32 time)
{
    d->eventTime = time;
}

void KeyboardInterface::updateModifiers(quint32 depressed, quint32 latched, quint32 locked, quint32 group)
{
    d->modifiers.depressed = depressed;
    d->modifiers.latched = latched;
    d->modifiers.locked = locked;
    d->modifiers.group = group;
    if (d->focusedSurface.surface && d->focusedSurface.keyboard) {
        d->sendModifiers(d->focusedSurface.keyboard);
    }
}

SurfaceInterface *KeyboardInterface::focusedSurface() const
{
    return d->focusedSurface.surface;
}

}
}

/*
    SPDX-FileCopyrightText: 2014 Martin Gräßlin <mgraesslin@kde.org>

    SPDX-License-Identifier: LGPL-2.1-only OR LGPL-3.0-only OR LicenseRef-KDE-Accepted-LGPL
*/
#include "keyboard_interface.h"
#include "keyboard_interface_p.h"
#include "display.h"
#include "seat_interface.h"
#include "surface_interface.h"
// Qt
#include <QTemporaryFile>
#include <QVector>
// Wayland
#include <wayland-server.h>

#include <unistd.h>

namespace KWayland
{

namespace Server
{

KeyboardInterface::Private::Private(SeatInterface *s, wl_resource *parentResource, KeyboardInterface *q)
    : Resource::Private(q, s, parentResource, &wl_keyboard_interface, &s_interface)
    , seat(s)
{
}

void KeyboardInterface::Private::focusChildSurface(const QPointer<SurfaceInterface> &childSurface, quint32 serial)
{
    if (focusedChildSurface == childSurface) {
        return;
    }
    sendLeave(focusedChildSurface.data(), serial);
    focusedChildSurface = childSurface;
    sendEnter(focusedChildSurface.data(), serial);
}

void KeyboardInterface::Private::sendLeave(SurfaceInterface *surface, quint32 serial)
{
    if (surface && resource && surface->resource()) {
        wl_keyboard_send_leave(resource, serial, surface->resource());
    }
}

void KeyboardInterface::Private::sendEnter(SurfaceInterface *surface, quint32 serial)
{
    wl_array keys;
    wl_array_init(&keys);
    const auto states = seat->pressedKeys();
    for (auto it = states.constBegin(); it != states.constEnd(); ++it) {
        uint32_t *k = reinterpret_cast<uint32_t*>(wl_array_add(&keys, sizeof(uint32_t)));
        *k = *it;
    }
    wl_keyboard_send_enter(resource, serial, surface->resource(), &keys);
    wl_array_release(&keys);

    sendModifiers();
}

#ifndef K_DOXYGEN
const struct wl_keyboard_interface KeyboardInterface::Private::s_interface {
    resourceDestroyedCallback
};
#endif

KeyboardInterface::KeyboardInterface(SeatInterface *parent, wl_resource *parentResource)
    : Resource(new Private(parent, parentResource, this))
{
}

KeyboardInterface::~KeyboardInterface() = default;

void KeyboardInterface::setKeymap(int fd, quint32 size)
{
    Q_D();
    d->sendKeymap(fd, size);
}

void KeyboardInterface::setKeymap(const QByteArray &content)
{
    QScopedPointer<QTemporaryFile> tmp{new QTemporaryFile(this)};
    if (!tmp->open()) {
        return;
    }
    unlink(tmp->fileName().toUtf8().constData());
    if (!tmp->resize(content.size())) {
        return;
    }
    uchar *address = tmp->map(0, content.size());
    if (!address) {
        return;
    }
    if (qstrncpy(reinterpret_cast<char*>(address), content.constData(), content.size() + 1) == nullptr) {
        return;
    }
    tmp->unmap(address);
    Q_D();
    d->sendKeymap(tmp->handle(), content.size());
    d->keymap.swap(tmp);
}

void KeyboardInterface::Private::sendKeymap(int fd, quint32 size)
{
    if (!resource) {
        return;
    }
    wl_keyboard_send_keymap(resource, WL_KEYBOARD_KEYMAP_FORMAT_XKB_V1, fd, size);
}

void KeyboardInterface::Private::sendModifiers(quint32 depressed, quint32 latched, quint32 locked, quint32 group, quint32 serial)
{
    if (!resource) {
        return;
    }
    wl_keyboard_send_modifiers(resource, serial, depressed, latched, locked, group);
}

void KeyboardInterface::Private::sendModifiers()
{
    sendModifiers(seat->depressedModifiers(), seat->latchedModifiers(), seat->lockedModifiers(), seat->groupModifiers(), seat->lastModifiersSerial());
}

void KeyboardInterface::setFocusedSurface(SurfaceInterface *surface, quint32 serial)
{
    Q_D();
    d->sendLeave(d->focusedChildSurface, serial);
    disconnect(d->destroyConnection);
    d->focusedChildSurface.clear();
    d->focusedSurface = surface;
    if (!d->focusedSurface) {
        return;
    }
    d->destroyConnection = connect(d->focusedSurface, &Resource::aboutToBeUnbound, this,
        [this] {
            Q_D();
            if (d->resource) {
                wl_keyboard_send_leave(d->resource, d->global->display()->nextSerial(), d->focusedSurface->resource());
            }
            d->focusedSurface = nullptr;
            d->focusedChildSurface.clear();
        }
    );
    d->focusedChildSurface = QPointer<SurfaceInterface>(surface);

    d->sendEnter(d->focusedSurface, serial);
    d->client->flush();
}

void KeyboardInterface::keyPressed(quint32 key, quint32 serial)
{
    Q_D();
    if (!d->resource) {
        return;
    }
    Q_ASSERT(d->focusedSurface);
    wl_keyboard_send_key(d->resource, serial, d->seat->timestamp(), key, WL_KEYBOARD_KEY_STATE_PRESSED);
}

void KeyboardInterface::keyReleased(quint32 key, quint32 serial)
{
    Q_D();
    if (!d->resource) {
        return;
    }
    Q_ASSERT(d->focusedSurface);
    wl_keyboard_send_key(d->resource, serial, d->seat->timestamp(), key, WL_KEYBOARD_KEY_STATE_RELEASED);
}

void KeyboardInterface::updateModifiers(quint32 depressed, quint32 latched, quint32 locked, quint32 group, quint32 serial)
{
    Q_D();
    Q_ASSERT(d->focusedSurface);
    d->sendModifiers(depressed, latched, locked, group, serial);
}

void KeyboardInterface::repeatInfo(qint32 charactersPerSecond, qint32 delay)
{
    Q_D();
    if (!d->resource) {
        return;
    }
    if (wl_resource_get_version(d->resource) < WL_KEYBOARD_REPEAT_INFO_SINCE_VERSION) {
        // only supported since version 4
        return;
    }
    wl_keyboard_send_repeat_info(d->resource, charactersPerSecond, delay);
}

SurfaceInterface *KeyboardInterface::focusedSurface() const
{
    Q_D();
    return d->focusedSurface;
}

KeyboardInterface::Private *KeyboardInterface::d_func() const
{
    return reinterpret_cast<Private*>(d.data());
}

}
}

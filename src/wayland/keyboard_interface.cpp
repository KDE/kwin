/*
    SPDX-FileCopyrightText: 2014 Martin Gräßlin <mgraesslin@kde.org>

    SPDX-License-Identifier: LGPL-2.1-only OR LGPL-3.0-only OR LicenseRef-KDE-Accepted-LGPL
*/
#include "display.h"
#include "keyboard_interface_p.h"
#include "seat_interface.h"
#include "seat_interface_p.h"
#include "surface_interface.h"
#include "utils/common.h"
// Qt
#include <QTemporaryFile>
#include <QVector>

#include <unistd.h>

namespace KWaylandServer
{
KeyboardInterfacePrivate::KeyboardInterfacePrivate(SeatInterface *s)
    : seat(s)
{
}

void KeyboardInterfacePrivate::keyboard_release(Resource *resource)
{
    wl_resource_destroy(resource->handle);
}

void KeyboardInterfacePrivate::keyboard_bind_resource(Resource *resource)
{
    const ClientConnection *focusedClient = focusedSurface ? focusedSurface->client() : nullptr;

    if (resource->version() >= WL_KEYBOARD_REPEAT_INFO_SINCE_VERSION) {
        send_repeat_info(resource->handle, keyRepeat.charactersPerSecond, keyRepeat.delay);
    }
    if (!keymap.isNull()) {
        sendKeymap(resource);
    }

    if (focusedClient && focusedClient->client() == resource->client()) {
        const QVector<quint32> keys = pressedKeys();
        const QByteArray keysData = QByteArray::fromRawData(reinterpret_cast<const char *>(keys.data()), sizeof(quint32) * keys.count());
        const quint32 serial = seat->display()->nextSerial();

        send_enter(resource->handle, serial, focusedSurface->resource(), keysData);
        send_modifiers(resource->handle, serial, modifiers.depressed, modifiers.latched, modifiers.locked, modifiers.group);
    }
}

QList<KeyboardInterfacePrivate::Resource *> KeyboardInterfacePrivate::keyboardsForClient(ClientConnection *client) const
{
    return resourceMap().values(client->client());
}

void KeyboardInterfacePrivate::sendLeave(SurfaceInterface *surface, quint32 serial)
{
    const QList<Resource *> keyboards = keyboardsForClient(surface->client());
    for (Resource *keyboardResource : keyboards) {
        send_leave(keyboardResource->handle, serial, surface->resource());
    }
}

void KeyboardInterfacePrivate::sendEnter(SurfaceInterface *surface, quint32 serial)
{
    const auto states = pressedKeys();
    QByteArray data = QByteArray::fromRawData(reinterpret_cast<const char *>(states.constData()), sizeof(quint32) * states.size());

    const QList<Resource *> keyboards = keyboardsForClient(surface->client());
    for (Resource *keyboardResource : keyboards) {
        send_enter(keyboardResource->handle, serial, surface->resource(), data);
    }
}

void KeyboardInterfacePrivate::sendKeymap(Resource *resource)
{
    std::unique_ptr<QTemporaryFile> tmp(new QTemporaryFile());
    if (!tmp->open()) {
        qCWarning(KWIN_CORE) << "Failed to create keymap file:" << tmp->errorString();
        return;
    }

    unlink(tmp->fileName().toUtf8().constData());
    if (!tmp->resize(keymap.size())) {
        qCWarning(KWIN_CORE) << "Failed to resize keymap file:" << tmp->errorString();
        return;
    }

    uchar *address = tmp->map(0, keymap.size());
    if (!address) {
        qCWarning(KWIN_CORE) << "Failed to map keymap file:" << tmp->errorString();
        return;
    }

    qstrncpy(reinterpret_cast<char *>(address), keymap.constData(), keymap.size() + 1);
    tmp->unmap(address);

    send_keymap(resource->handle, keymap_format::keymap_format_xkb_v1, tmp->handle(), tmp->size());
}

void KeyboardInterface::setKeymap(const QByteArray &content)
{
    if (content.isNull()) {
        return;
    }

    d->keymap = content;

    const auto keyboardResources = d->resourceMap();
    for (KeyboardInterfacePrivate::Resource *resource : keyboardResources) {
        d->sendKeymap(resource);
    }
}

void KeyboardInterfacePrivate::sendModifiers(quint32 depressed, quint32 latched, quint32 locked, quint32 group, quint32 serial)
{
    const QList<Resource *> keyboards = keyboardsForClient(focusedSurface->client());
    for (Resource *keyboardResource : keyboards) {
        send_modifiers(keyboardResource->handle, serial, depressed, latched, locked, group);
    }
}

bool KeyboardInterfacePrivate::updateKey(quint32 key, KeyboardKeyState state)
{
    auto it = states.find(key);
    if (it == states.end()) {
        states.insert(key, state);
        return true;
    }
    if (it.value() == state) {
        return false;
    }
    it.value() = state;
    return true;
}

KeyboardInterface::KeyboardInterface(SeatInterface *seat)
    : d(new KeyboardInterfacePrivate(seat))
{
}

KeyboardInterface::~KeyboardInterface() = default;

void KeyboardInterfacePrivate::sendModifiers()
{
    sendModifiers(modifiers.depressed, modifiers.latched, modifiers.locked, modifiers.group, modifiers.serial);
}

void KeyboardInterface::setFocusedSurface(SurfaceInterface *surface, quint32 serial)
{
    if (d->focusedSurface == surface) {
        return;
    }

    if (d->focusedSurface) {
        d->sendLeave(d->focusedSurface, serial);
        disconnect(d->destroyConnection);
    }

    d->focusedSurface = surface;
    if (!d->focusedSurface) {
        return;
    }
    d->destroyConnection = connect(d->focusedSurface, &SurfaceInterface::aboutToBeDestroyed, this, [this] {
        d->sendLeave(d->focusedSurface, d->seat->display()->nextSerial());
        d->focusedSurface = nullptr;
    });

    d->sendEnter(d->focusedSurface, serial);
    d->sendModifiers();
}

QVector<quint32> KeyboardInterfacePrivate::pressedKeys() const
{
    QVector<quint32> keys;
    for (auto it = states.constBegin(); it != states.constEnd(); ++it) {
        if (it.value() == KeyboardKeyState::Pressed) {
            keys << it.key();
        }
    }
    return keys;
}

void KeyboardInterface::sendKey(quint32 key, KeyboardKeyState state)
{
    if (!d->updateKey(key, state)) {
        return;
    }

    if (!d->focusedSurface) {
        return;
    }

    const QList<KeyboardInterfacePrivate::Resource *> keyboards = d->keyboardsForClient(d->focusedSurface->client());
    const quint32 serial = d->seat->display()->nextSerial();
    for (KeyboardInterfacePrivate::Resource *keyboardResource : keyboards) {
        d->send_key(keyboardResource->handle, serial, d->seat->timestamp(), key, quint32(state));
    }
}

void KeyboardInterface::sendModifiers(quint32 depressed, quint32 latched, quint32 locked, quint32 group)
{
    bool changed = false;
#define UPDATE(value)                  \
    if (d->modifiers.value != value) { \
        d->modifiers.value = value;    \
        changed = true;                \
    }
    UPDATE(depressed)
    UPDATE(latched)
    UPDATE(locked)
    UPDATE(group)
    if (!changed) {
        return;
    }

    if (!d->focusedSurface) {
        return;
    }

    d->modifiers.serial = d->seat->display()->nextSerial();
    d->sendModifiers(depressed, latched, locked, group, d->modifiers.serial);
}

void KeyboardInterface::setRepeatInfo(qint32 charactersPerSecond, qint32 delay)
{
    d->keyRepeat.charactersPerSecond = qMax(charactersPerSecond, 0);
    d->keyRepeat.delay = qMax(delay, 0);
    const QList<KeyboardInterfacePrivate::Resource *> keyboards = d->resourceMap().values();
    for (KeyboardInterfacePrivate::Resource *keyboardResource : keyboards) {
        if (keyboardResource->version() >= WL_KEYBOARD_REPEAT_INFO_SINCE_VERSION) {
            d->send_repeat_info(keyboardResource->handle, d->keyRepeat.charactersPerSecond, d->keyRepeat.delay);
        }
    }
}

SurfaceInterface *KeyboardInterface::focusedSurface() const
{
    return d->focusedSurface;
}

qint32 KeyboardInterface::keyRepeatDelay() const
{
    return d->keyRepeat.delay;
}

qint32 KeyboardInterface::keyRepeatRate() const
{
    return d->keyRepeat.charactersPerSecond;
}

}

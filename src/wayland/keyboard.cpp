/*
    SPDX-FileCopyrightText: 2014 Martin Gräßlin <mgraesslin@kde.org>

    SPDX-License-Identifier: LGPL-2.1-only OR LGPL-3.0-only OR LicenseRef-KDE-Accepted-LGPL
*/
#include "clientconnection.h"
#include "display.h"
#include "keyboard_p.h"
#include "seat.h"
#include "seat_p.h"
#include "surface.h"
#include "utils/common.h"
// Qt
#include <QList>

#include <unistd.h>

namespace KWin
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
        const QByteArray keysData = QByteArray::fromRawData(reinterpret_cast<const char *>(pressedKeys.data()), sizeof(quint32) * pressedKeys.count());
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
    QByteArray data = QByteArray::fromRawData(reinterpret_cast<const char *>(pressedKeys.constData()), sizeof(quint32) * pressedKeys.size());

    const QList<Resource *> keyboards = keyboardsForClient(surface->client());
    for (Resource *keyboardResource : keyboards) {
        send_enter(keyboardResource->handle, serial, surface->resource(), data);
    }
}

void KeyboardInterfacePrivate::sendKeymap(Resource *resource)
{
    // From version 7 on, keymaps must be mapped privately, so that
    // we can seal the fd and reuse it between clients.
    if (resource->version() >= 7 && sharedKeymapFile.effectiveFlags().testFlag(RamFile::Flag::SealWrite)) {
        send_keymap(resource->handle, keymap_format::keymap_format_xkb_v1, sharedKeymapFile.fd(), sharedKeymapFile.size());
        // otherwise give each client its own unsealed copy.
    } else {
        RamFile keymapFile("kwin-xkb-keymap", keymap.constData(), keymap.size() + 1); // Include QByteArray null-terminator.
        send_keymap(resource->handle, keymap_format::keymap_format_xkb_v1, keymapFile.fd(), keymapFile.size());
    }
}

void KeyboardInterface::setKeymap(const QByteArray &content)
{
    if (content.isNull()) {
        return;
    }

    d->keymap = content;
    // +1 to include QByteArray null terminator.
    d->sharedKeymapFile = RamFile("kwin-xkb-keymap-shared", content.constData(), content.size() + 1, RamFile::Flag::SealWrite);

    const auto keyboardResources = d->resourceMap();
    for (KeyboardInterfacePrivate::Resource *resource : keyboardResources) {
        d->sendKeymap(resource);
    }
}

void KeyboardInterfacePrivate::sendModifiers(SurfaceInterface *surface, quint32 depressed, quint32 latched, quint32 locked, quint32 group, quint32 serial)
{
    const QList<Resource *> keyboards = keyboardsForClient(surface->client());
    for (Resource *keyboardResource : keyboards) {
        send_modifiers(keyboardResource->handle, serial, depressed, latched, locked, group);
    }
}

bool KeyboardInterfacePrivate::updateKey(quint32 key, KeyboardKeyState state)
{
    switch (state) {
    case KeyboardKeyState::Pressed:
        if (pressedKeys.contains(key)) {
            return false;
        }
        pressedKeys.append(key);
        return true;
    case KeyboardKeyState::Released:
        return pressedKeys.removeOne(key);
    default:
        Q_UNREACHABLE();
    }
}

KeyboardInterface::KeyboardInterface(SeatInterface *seat)
    : d(new KeyboardInterfacePrivate(seat))
{
}

KeyboardInterface::~KeyboardInterface() = default;

void KeyboardInterfacePrivate::sendModifiers(SurfaceInterface *surface)
{
    sendModifiers(surface, modifiers.depressed, modifiers.latched, modifiers.locked, modifiers.group, modifiers.serial);
}

void KeyboardInterface::setFocusedSurface(SurfaceInterface *surface, const QList<quint32> &keys, quint32 serial)
{
    if (d->focusedSurface == surface) {
        return;
    }

    if (d->focusedSurface) {
        d->sendLeave(d->focusedSurface, serial);
        disconnect(d->destroyConnection);
    }

    d->focusedSurface = surface;
    d->pressedKeys = keys;

    if (!d->focusedSurface) {
        return;
    }
    d->destroyConnection = connect(d->focusedSurface, &SurfaceInterface::aboutToBeDestroyed, this, [this] {
        d->sendLeave(d->focusedSurface, d->seat->display()->nextSerial());
        d->focusedSurface = nullptr;
    });

    d->sendEnter(d->focusedSurface, serial);
    d->sendModifiers(d->focusedSurface);
}

void KeyboardInterface::setModifierFocusSurface(SurfaceInterface *surface)
{
    if (d->modifierFocusSurface == surface) {
        return;
    }
    d->modifierFocusSurface = surface;
    if (d->modifierFocusSurface && d->focusedSurface != d->modifierFocusSurface) {
        d->modifiers.serial = d->seat->display()->nextSerial();
        d->sendModifiers(d->modifierFocusSurface);
    }
}

void KeyboardInterface::sendKey(quint32 key, KeyboardKeyState state, ClientConnection *client)
{
    quint32 waylandState;
    switch (state) {
    case KeyboardKeyState::Pressed:
        waylandState = WL_KEYBOARD_KEY_STATE_PRESSED;
        break;
    case KeyboardKeyState::Released:
        waylandState = WL_KEYBOARD_KEY_STATE_RELEASED;
        break;
    case KeyboardKeyState::Repeated:
        Q_UNREACHABLE();
    }

    const QList<KeyboardInterfacePrivate::Resource *> keyboards = d->keyboardsForClient(client);
    const quint32 serial = d->seat->display()->nextSerial();
    for (KeyboardInterfacePrivate::Resource *keyboardResource : keyboards) {
        d->send_key(keyboardResource->handle, serial, d->seat->timestamp().count(), key, waylandState);
    }
}

void KeyboardInterface::sendKey(quint32 key, KeyboardKeyState state)
{
    if (!d->focusedSurface) {
        return;
    }

    if (!d->updateKey(key, state)) {
        return;
    }

    sendKey(key, state, d->focusedSurface->client());
}

void KeyboardInterface::sendModifiers(quint32 depressed, quint32 latched, quint32 locked, quint32 group)
{
    bool changed = false;
    if (d->modifiers.depressed != depressed) {
        d->modifiers.depressed = depressed;
        changed = true;
    }
    if (d->modifiers.latched != latched) {
        d->modifiers.latched = latched;
        changed = true;
    }
    if (d->modifiers.locked != locked) {
        d->modifiers.locked = locked;
        changed = true;
    }
    if (d->modifiers.group != group) {
        d->modifiers.group = group;
        changed = true;
    }
    if (!changed) {
        return;
    }

    if (d->focusedSurface) {
        d->modifiers.serial = d->seat->display()->nextSerial();
        d->sendModifiers(d->focusedSurface, depressed, latched, locked, group, d->modifiers.serial);
    }
    if (d->modifierFocusSurface && d->focusedSurface != d->modifierFocusSurface) {
        d->modifiers.serial = d->seat->display()->nextSerial();
        d->sendModifiers(d->modifierFocusSurface, depressed, latched, locked, group, d->modifiers.serial);
    }
}

void KeyboardInterface::sendModifiers(quint32 depressed, quint32 latched, quint32 locked, quint32 group, ClientConnection *client)
{
    const QList<KeyboardInterfacePrivate::Resource *> keyboards = d->keyboardsForClient(client);
    const quint32 serial = d->seat->display()->nextSerial();
    for (KeyboardInterfacePrivate::Resource *keyboardResource : keyboards) {
        d->send_modifiers(keyboardResource->handle, serial, depressed, latched, locked, group);
    }
}

void KeyboardInterface::setRepeatInfo(qint32 charactersPerSecond, qint32 delay)
{
    d->keyRepeat.charactersPerSecond = std::max(charactersPerSecond, 0);
    d->keyRepeat.delay = std::max(delay, 0);
    const auto keyboards = d->resourceMap();
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

#include "moc_keyboard.cpp"

/*
    SPDX-FileCopyrightText: 2025 Nicolas Fella <nicolas.fella@gmx.de>

    SPDX-License-Identifier: GPL-2.0-only OR GPL-3.0-only OR LicenseRef-KDE-Accepted-GPL
*/

#include "a11ykeyboardmonitor.h"
#include "keyboard_input.h"
#include "wayland/keyboard.h"
#include "wayland/seat.h"
#include "wayland_server.h"
#include "xkb.h"

#include <QDBusConnection>
#include <QDBusConnectionInterface>
#include <QDBusMetaType>
#include <QDBusServiceWatcher>

using namespace std::literals;

namespace KWin
{

A11yKeyboardMonitor::A11yKeyboardMonitor()
    : QObject()
{
    qDBusRegisterMetaType<A11yKeyboardMonitor::KeyStroke>();
    qDBusRegisterMetaType<QList<A11yKeyboardMonitor::KeyStroke>>();

    m_dbusWatcher.setConnection(QDBusConnection::sessionBus());
    m_dbusWatcher.setWatchMode(QDBusServiceWatcher::WatchForUnregistration);

    connect(&m_dbusWatcher, &QDBusServiceWatcher::serviceUnregistered, this, [this](const QString &serviceName) {
        m_clients.remove(serviceName);
    });

    QDBusConnection::sessionBus().registerObject(QStringLiteral("/org/freedesktop/a11y/Manager"), this, QDBusConnection::ExportScriptableContents);
    QDBusConnection::sessionBus().registerService(QStringLiteral("org.freedesktop.a11y.Manager"));
}

bool A11yKeyboardMonitor::processKey(uint32_t key, KeyboardKeyState state, std::chrono::microseconds time)
{
    const auto mods = xkb_state_serialize_mods(input()->keyboard()->xkb()->state(), xkb_state_component(XKB_STATE_MODS_EFFECTIVE));

    const auto keysym = input()->keyboard()->xkb()->toKeysym(key);
    const auto text = input()->keyboard()->xkb()->toString(keysym);
    const quint32 unicode = text.isEmpty() ? 0 : text.at(0).unicode();
    const bool released = state == KeyboardKeyState::Released;
    const auto keycode = key + 8;

    for (auto [name, data] : m_clients.asKeyValueRange()) {
        if (data.grabbed) {
            emitKeyEvent(name, released, mods, keysym, unicode, keycode);
        }

        // if any of the grabbed modifiers is currently down, grab the key
        for (const auto grabbedMod : data.modifiers) {
            if (grabbedMod != keysym && data.pressedModifiers.contains(grabbedMod)) {
                emitKeyEvent(name, released, mods, keysym, unicode, keycode);
                return true;
            }
        }

        // if the modifier was pressed twice within the key repeat delay process it normally
        const qint32 keyRepeatDelay = waylandServer()->seat()->keyboard()->keyRepeatDelay();
        if (state == KeyboardKeyState::Pressed && data.lastModifier == keysym && time < data.lastModifierTime + std::chrono::milliseconds(keyRepeatDelay)) {
            emitKeyEvent(name, released, mods, keysym, unicode, keycode);
            data.modifierWasForwarded = true;
            return false;
        }

        // if the modifier press was forwarded also forward the release
        if (state == KeyboardKeyState::Released && data.modifierWasForwarded) {
            emitKeyEvent(name, released, mods, keysym, unicode, keycode);
            data.modifierWasForwarded = false;
            return false;
        }

        if (data.modifiers.contains(keysym)) {
            data.lastModifier = keysym;
            data.lastModifierTime = time;

            if (released) {
                data.pressedModifiers.remove(keysym);
            } else {
                data.pressedModifiers.insert(keysym);
            }

            emitKeyEvent(name, released, mods, keysym, unicode, keycode);
            return true;
        }

        for (const KeyStroke &stroke : std::as_const(data.keys)) {
            if (mods == stroke.modifiers && stroke.keysym == keysym) {
                emitKeyEvent(name, released, mods, keysym, unicode, keycode);
                return true;
            }
        }

        if (data.watched) {
            emitKeyEvent(name, released, mods, keysym, unicode, keycode);
        }
    }

    return false;
}

void A11yKeyboardMonitor::GrabKeyboard()
{
    if (!checkPermission()) {
        return;
    }

    m_dbusWatcher.addWatchedService(message().service());

    m_clients[message().service()].grabbed = true;
}

void A11yKeyboardMonitor::UngrabKeyboard()
{
    if (!checkPermission()) {
        return;
    }

    if (!m_clients.contains(message().service())) {
        return;
    }

    m_clients[message().service()].grabbed = false;
}

void A11yKeyboardMonitor::WatchKeyboard()
{
    if (!checkPermission()) {
        return;
    }

    m_dbusWatcher.addWatchedService(message().service());

    m_clients[message().service()].watched = true;
}

void A11yKeyboardMonitor::UnwatchKeyboard()
{
    if (!checkPermission()) {
        return;
    }

    if (!m_clients.contains(message().service())) {
        return;
    }

    m_clients[message().service()].watched = false;
}

void A11yKeyboardMonitor::SetKeyGrabs(const QList<quint32> &modifiers, const QList<A11yKeyboardMonitor::KeyStroke> &keystrokes)
{
    if (!checkPermission()) {
        return;
    }

    m_dbusWatcher.addWatchedService(message().service());

    m_clients[message().service()].modifiers = modifiers;
    m_clients[message().service()].keys = keystrokes;
    m_clients[message().service()].pressedModifiers.clear();
}

void A11yKeyboardMonitor::emitKeyEvent(const QString &name, bool released, quint32 state, quint32 keysym, quint32 unichar, quint16 keycode)
{
    QDBusMessage signal = QDBusMessage::createTargetedSignal(name, QStringLiteral("/org/freedesktop/a11y/Manager"), QStringLiteral("org.freedesktop.a11y.KeyboardMonitor"), QStringLiteral("KeyEvent"));
    QVariant keycodeVariant;
    keycodeVariant.setValue(keycode);
    signal.setArguments({released, state, keysym, unichar, keycodeVariant});
    QDBusConnection::sessionBus().call(signal, QDBus::NoBlock);
}

bool A11yKeyboardMonitor::checkPermission()
{
    QDBusMessage msg = QDBusMessage::createMethodCall(QStringLiteral("org.freedesktop.DBus"), QStringLiteral("/org/freedesktop/DBus"), QStringLiteral("org.freedesktop.DBus"), "GetNameOwner");
    msg.setArguments({QStringLiteral("org.gnome.Orca.KeyboardMonitor")});
    QDBusReply<QString> orcaName = QDBusConnection::sessionBus().call(msg);

    if (message().service() != orcaName) {
        sendErrorReply(QDBusError::AccessDenied, "Only screen readers are allowed to use this interface");
        return false;
    }

    return true;
}

const QDBusArgument &operator>>(const QDBusArgument &arg, A11yKeyboardMonitor::KeyStroke &keystroke)
{
    arg.beginStructure();
    arg >> keystroke.keysym;
    arg >> keystroke.modifiers;
    arg.endStructure();

    return arg;
}

const QDBusArgument &operator<<(QDBusArgument &arg, const A11yKeyboardMonitor::KeyStroke &keystroke)
{
    arg.beginStructure();
    arg << keystroke.keysym;
    arg << keystroke.modifiers;
    arg.endStructure();

    return arg;
}

}

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

using namespace Qt::Literals;
using namespace std::literals;

A11yKeyboardMonitor::A11yKeyboardMonitor()
    : KWin::InputEventFilter(KWin::InputFilterOrder::A11yKeyboardMonitor)
{
    KWin::input()->installInputEventFilter(this);

    qDBusRegisterMetaType<A11yKeyboardMonitor::KeyStroke>();
    qDBusRegisterMetaType<QList<A11yKeyboardMonitor::KeyStroke>>();

    QDBusConnection::sessionBus().registerService(u"org.freedesktop.a11y.Manager"_s);
    QDBusConnection::sessionBus().registerObject(u"/org/freedesktop/a11y/Manager"_s, this, QDBusConnection::ExportScriptableContents);
}

bool A11yKeyboardMonitor::keyboardKeyPreXkb(KWin::KeyboardKeyEventPreXkb *event)
{
    const auto mods = xkb_state_serialize_mods(KWin::input()->keyboard()->xkb()->state(), xkb_state_component(XKB_STATE_MODS_EFFECTIVE));

    const auto keysym = KWin::input()->keyboard()->xkb()->toKeysym(event->scanCode);
    const auto text = KWin::input()->keyboard()->xkb()->toString(keysym);
    const quint32 unicode = text.isEmpty() ? 0 : text.at(0).unicode();
    const bool released = event->state == KWin::KeyboardKeyState::Released;

    for (auto [name, data] : m_clients.asKeyValueRange()) {
        if (data.grabbed) {
            emitKeyevent(name, released, mods, keysym, unicode, event->scanCode);
        }

        // if the modifier was pressed twice within the key repeat delay process it normally
        const qint32 keyRepeatDelay = KWin::waylandServer()->seat()->keyboard()->keyRepeatDelay();
        if (event->state == KWin::KeyboardKeyState::Pressed && data.lastModifier == keysym && event->timestamp < data.lastModifierTime + std::chrono::milliseconds(keyRepeatDelay)) {
            data.modifierWasForwarded = true;
            return false;
        }

        // if the modifier press was forwarded also forward the release
        if (event->state == KWin::KeyboardKeyState::Released && data.modifierWasForwarded) {
            data.modifierWasForwarded = false;
            return false;
        }

        data.lastModifier = keysym;
        data.lastModifierTime = event->timestamp;

        if (data.modifiers.contains(keysym)) {
            emitKeyevent(name, released, mods, keysym, unicode, event->scanCode);
            return true;
        }

        for (const KeyStroke &stroke : std::as_const(data.keys)) {
            if (mods == stroke.modifiers && stroke.keysym == keysym) {
                emitKeyevent(name, released, mods, keysym, unicode, event->scanCode);
                return true;
            }
        }

        if (data.watched) {
            emitKeyevent(name, released, mods, keysym, unicode, event->scanCode);
        }
    }

    return false;
}

void A11yKeyboardMonitor::GrabKeyboard()
{
    if (!checkPermission()) {
        return;
    }

    watchForUnregistration();

    m_clients[message().service()].grabbed = true;
}

void A11yKeyboardMonitor::UngrabKeyboard()
{
    if (!checkPermission()) {
        return;
    }

    m_clients[message().service()].grabbed = false;
}

void A11yKeyboardMonitor::WatchKeyboard()
{
    if (!checkPermission()) {
        return;
    }

    watchForUnregistration();

    m_clients[message().service()].watched = true;
}

void A11yKeyboardMonitor::UnwatchKeyboard()
{
    if (!checkPermission()) {
        return;
    }
    m_clients[message().service()].watched = false;
}

void A11yKeyboardMonitor::SetKeyGrabs(const QList<quint32> &modifiers, const QList<A11yKeyboardMonitor::KeyStroke> &keystrokes)
{
    if (!checkPermission()) {
        return;
    }

    watchForUnregistration();

    QDBusMessage msg = QDBusMessage::createMethodCall(u"org.freedesktop.DBus"_s, u"/org/freedesktop/DBus"_s, u"org.freedesktop.DBus"_s, "GetNameOwner");
    msg.setArguments({u"org.gnome.Orca.KeyboardMonitor"_s});
    QDBusReply<QString> r = QDBusConnection::sessionBus().call(msg);

    m_clients[message().service()].modifiers = modifiers;
    m_clients[message().service()].keys = keystrokes;
}

void A11yKeyboardMonitor::emitKeyevent(const QString &name, bool released, quint32 state, quint32 keysym, quint32 unichar, quint16 keycode)
{
    QDBusMessage signal = QDBusMessage::createTargetedSignal(name, u"/org/freedesktop/a11y/Manager"_s, u"org.freedesktop.a11y.KeyboardMonitor"_s, u"KeyEvent"_s);
    QVariant keycodeVariant;
    keycodeVariant.setValue(keycode);
    signal.setArguments({released, state, keysym, unichar, keycodeVariant});
    QDBusConnection::sessionBus().call(signal, QDBus::NoBlock);
}

bool A11yKeyboardMonitor::checkPermission()
{
    QDBusMessage msg = QDBusMessage::createMethodCall(u"org.freedesktop.DBus"_s, u"/org/freedesktop/DBus"_s, u"org.freedesktop.DBus"_s, "GetNameOwner");
    msg.setArguments({u"org.gnome.Orca.KeyboardMonitor"_s});
    QDBusReply<QString> orcaName = QDBusConnection::sessionBus().call(msg);

    if (message().service() != orcaName) {
        sendErrorReply(QDBusError::AccessDenied, "Only screen readers are allowed to use this interface");
        return false;
    }

    return true;
}

void A11yKeyboardMonitor::watchForUnregistration()
{
    if (m_clients.contains(message().service())) {
        return;
    }

    QDBusServiceWatcher *watcher = new QDBusServiceWatcher(message().service(), connection(), QDBusServiceWatcher::WatchForUnregistration);

    connect(watcher, &QDBusServiceWatcher::serviceUnregistered, this, [this](const QString &service) {
        m_clients.remove(service);
    });
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

#include "moc_a11ykeyboardmonitor.cpp"

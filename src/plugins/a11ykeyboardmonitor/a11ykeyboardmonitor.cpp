/*
    SPDX-FileCopyrightText: 2025 Nicolas Fella <nicolas.fella@gmx.de>

    SPDX-License-Identifier: GPL-2.0-only OR GPL-3.0-only OR LicenseRef-KDE-Accepted-GPL
*/

#include "a11ykeyboardmonitor.h"
#include "keyboard_input.h"
#include "xkb.h"

#include <QDBusConnection>
#include <QDBusConnectionInterface>
#include <QDBusMetaType>

using namespace Qt::Literals;

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
    auto text = KWin::input()->keyboard()->xkb()->toString(keysym);
    quint32 unicode = text.isEmpty() ? 0 : text.at(0).unicode();
    const bool released = event->state == KWin::KeyboardKeyState::Released;

    if (m_keyboardGrabbed) {
        Q_EMIT KeyEvent(released, mods, keysym, unicode, event->scanCode);
        return true;
    }

    if (m_grabbedModifiers.contains(keysym)) {
        Q_EMIT KeyEvent(released, mods, keysym, unicode, event->scanCode);
        return true;
    }

    for (const KeyStroke &stroke : std::as_const(m_grabbedKeystrokes)) {
        if (mods == stroke.modifiers && stroke.keysym == keysym) {
            Q_EMIT KeyEvent(released, mods, keysym, unicode, event->scanCode);
            return true;
        }
    }

    if (m_keyboardWatched) {
        Q_EMIT KeyEvent(released, mods, keysym, unicode, event->scanCode);
    }

    return false;
}

void A11yKeyboardMonitor::GrabKeyboard()
{
    m_keyboardGrabbed = true;
}

void A11yKeyboardMonitor::UngrabKeyboard()
{
    m_keyboardGrabbed = false;
}

void A11yKeyboardMonitor::WatchKeyboard()
{
    m_keyboardWatched = true;
}

void A11yKeyboardMonitor::UnwatchKeyboard()
{
    m_keyboardWatched = false;
}

void A11yKeyboardMonitor::SetKeyGrabs(const QList<quint32> &modifiers, const QList<A11yKeyboardMonitor::KeyStroke> &keystrokes)
{
    m_grabbedModifiers = modifiers;
    m_grabbedKeystrokes = keystrokes;
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

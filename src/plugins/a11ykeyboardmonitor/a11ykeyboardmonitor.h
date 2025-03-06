/*
    SPDX-FileCopyrightText: 2025 Nicolas Fella <nicolas.fella@gmx.de>

    SPDX-License-Identifier: GPL-2.0-only OR GPL-3.0-only OR LicenseRef-KDE-Accepted-GPL
*/

#pragma once

#include "plugin.h"

#include "input.h"
#include "input_event.h"

#include <QDBusArgument>
#include <QDBusContext>

class A11yKeyboardMonitor : public KWin::Plugin, public KWin::InputEventFilter, protected QDBusContext
{
    Q_OBJECT
    Q_CLASSINFO("D-Bus Interface", "org.freedesktop.a11y.KeyboardMonitor")
public:
    explicit A11yKeyboardMonitor();

    struct KeyStroke
    {
        quint32 keysym;
        quint32 modifiers;
    };

    Q_SCRIPTABLE void GrabKeyboard();

    Q_SCRIPTABLE void UngrabKeyboard();

    Q_SCRIPTABLE void WatchKeyboard();

    Q_SCRIPTABLE void UnwatchKeyboard();

    Q_SCRIPTABLE void SetKeyGrabs(const QList<quint32> &modifiers, const QList<KeyStroke> &keystrokes);

Q_SIGNALS:
    Q_SCRIPTABLE void KeyEvent(bool released, quint32 state, quint32 keysym, quint32 unichar, quint16 keycode);

protected:
    bool keyboardKeyPreXkb(KWin::KeyboardKeyEventPreXkb *event) override;

private:
    bool m_keyboardWatched = false;
    bool m_keyboardGrabbed = false;
    QList<quint32> m_grabbedModifiers;
    QList<KeyStroke> m_grabbedKeystrokes;
};

const QDBusArgument &operator>>(const QDBusArgument &arg, A11yKeyboardMonitor::KeyStroke &keystroke);
const QDBusArgument &operator<<(QDBusArgument &arg, const A11yKeyboardMonitor::KeyStroke &keystroke);

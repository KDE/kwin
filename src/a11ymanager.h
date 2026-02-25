/*
    SPDX-FileCopyrightText: 2025 Nicolas Fella <nicolas.fella@gmx.de>

    SPDX-License-Identifier: GPL-2.0-only OR GPL-3.0-only OR LicenseRef-KDE-Accepted-GPL
*/

#pragma once

#include "input.h"
#include "input_event.h"

#include <QDBusArgument>
#include <QDBusContext>
#include <QDBusServiceWatcher>
#include <QObject>

namespace KWin
{

class A11yManager : public QObject, protected QDBusContext
{
    Q_OBJECT

public:
    explicit A11yManager();

    struct KeyStroke
    {
        quint32 keysym;
        quint32 modifiers;
    };

    void GrabKeyboard();

    void UngrabKeyboard();

    void WatchKeyboard();

    void UnwatchKeyboard();

    void SetKeyGrabs(const QList<quint32> &modifiers, const QList<KeyStroke> &keystrokes);

Q_SIGNALS:
    void KeyEvent(bool released, quint32 state, quint32 keysym, quint32 unichar, quint16 keycode);

public:
    bool processKey(uint32_t key, KeyboardKeyState state, std::chrono::microseconds time);

private:
    bool checkPermission();
    void emitKeyEvent(const QString &name, bool released, quint32 state, quint32 keysym, quint32 unichar, quint16 keycode);

    struct GrabData
    {
        bool watched;
        bool grabbed;
        QList<quint32> modifiers;
        QList<KeyStroke> keys;
        quint32 lastModifier;
        std::chrono::microseconds lastModifierTime;
        bool modifierWasForwarded = false;
        QSet<quint32> pressedModifiers;
    };

    QHash<QString, GrabData> m_clients;
    QDBusServiceWatcher m_dbusWatcher;
};

const QDBusArgument &operator>>(const QDBusArgument &arg, A11yManager::KeyStroke &keystroke);
const QDBusArgument &operator<<(QDBusArgument &arg, const A11yManager::KeyStroke &keystroke);

}

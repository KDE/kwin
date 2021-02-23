/*
    KWin - the KDE window manager
    This file is part of the KDE project.

    SPDX-FileCopyrightText: 2017 Martin Flöser <mgraesslin@kde.org>

    SPDX-License-Identifier: GPL-2.0-or-later
*/
#include "virtualkeyboard_dbus.h"
#include <QDBusConnection>

namespace KWin
{

VirtualKeyboardDBus::VirtualKeyboardDBus(QObject *parent)
    : QObject(parent)
{
    QDBusConnection::sessionBus().registerObject(QStringLiteral("/VirtualKeyboard"), this,
                                                 QDBusConnection::ExportAllProperties |
                                                 QDBusConnection::ExportScriptableSignals | //qdbuscpp2xml doesn't support yet properties with NOTIFY
                                                 QDBusConnection::ExportAllSlots);
}

VirtualKeyboardDBus::~VirtualKeyboardDBus() = default;

bool VirtualKeyboardDBus::isActive() const
{
    return m_active;
}

void VirtualKeyboardDBus::requestEnabled(bool enabled)
{
    emit enableRequested(enabled);
}

void VirtualKeyboardDBus::setActive(bool active)
{
    if (m_active != active) {
        m_active = active;
        Q_EMIT activeChanged();
    }
}

void VirtualKeyboardDBus::hide()
{
    Q_EMIT hideRequested();
}

bool VirtualKeyboardDBus::isEnabled() const
{
    return m_enabled;
}

void VirtualKeyboardDBus::setEnabled(bool enabled)
{
    if (m_enabled == enabled) {
        return;
    }
    m_enabled = enabled;
    emit enabledChanged();
}

}

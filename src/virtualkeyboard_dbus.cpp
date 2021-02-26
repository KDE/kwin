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

VirtualKeyboardDBus::VirtualKeyboardDBus(InputMethod *parent)
    : QObject(parent)
    , m_inputMethod(parent)
{
    QDBusConnection::sessionBus().registerObject(QStringLiteral("/VirtualKeyboard"), this,
                                                 QDBusConnection::ExportAllProperties |
                                                 QDBusConnection::ExportScriptableSignals | //qdbuscpp2xml doesn't support yet properties with NOTIFY
                                                 QDBusConnection::ExportAllSlots);
    connect(parent, &InputMethod::activeChanged, this, &VirtualKeyboardDBus::activeChanged);
    connect(parent, &InputMethod::enabledChanged, this, &VirtualKeyboardDBus::enabledChanged);
}

VirtualKeyboardDBus::~VirtualKeyboardDBus() = default;

bool VirtualKeyboardDBus::isActive() const
{
    return m_inputMethod->isActive();
}

void VirtualKeyboardDBus::setEnabled(bool enabled)
{
    m_inputMethod->setEnabled(enabled);
}

void VirtualKeyboardDBus::setActive(bool active)
{
    m_inputMethod->setActive(active);
}

bool VirtualKeyboardDBus::isEnabled() const
{
    return m_inputMethod->isEnabled();
}

}

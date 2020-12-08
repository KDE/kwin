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
                                                 QDBusConnection::ExportAllInvokables |
                                                 QDBusConnection::ExportScriptableSignals |
                                                 QDBusConnection::ExportAllSlots);
}

VirtualKeyboardDBus::~VirtualKeyboardDBus() = default;

}

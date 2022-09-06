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
                                                 QDBusConnection::ExportAllProperties | QDBusConnection::ExportScriptableContents | // qdbuscpp2xml doesn't support yet properties with NOTIFY
                                                     QDBusConnection::ExportAllSlots);
    connect(parent, &InputMethod::activeChanged, this, &VirtualKeyboardDBus::activeChanged);
    connect(parent, &InputMethod::enabledChanged, this, &VirtualKeyboardDBus::enabledChanged);
    connect(parent, &InputMethod::visibleChanged, this, &VirtualKeyboardDBus::visibleChanged);
    connect(parent, &InputMethod::availableChanged, this, &VirtualKeyboardDBus::availableChanged);
    connect(parent, &InputMethod::activeClientSupportsTextInputChanged, this, &VirtualKeyboardDBus::activeClientSupportsTextInputChanged);
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

bool VirtualKeyboardDBus::isVisible() const
{
    return m_inputMethod->isVisible();
}

bool VirtualKeyboardDBus::isAvailable() const
{
    return m_inputMethod->isAvailable();
}

bool VirtualKeyboardDBus::activeClientSupportsTextInput() const
{
    return m_inputMethod->activeClientSupportsTextInput();
}

bool VirtualKeyboardDBus::willShowOnActive() const
{
    return isAvailable() && isEnabled() && m_inputMethod->shouldShowOnActive();
}

void VirtualKeyboardDBus::forceActivate()
{
    m_inputMethod->forceActivate();
}
}

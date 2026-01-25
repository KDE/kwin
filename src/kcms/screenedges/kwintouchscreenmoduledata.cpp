/*
    KWin - the KDE window manager
    This file is part of the KDE project.

    SPDX-FileCopyrightText: 2026 Alexander Wilms <f.alexander.wilms@outlook.com>

    SPDX-License-Identifier: GPL-2.0-or-later
*/

#include "kwintouchscreenmoduledata.h"

#include "inputdevicemanager_interface.h"

#include <QDBusConnection>

namespace KWin
{

KWinTouchScreenModuleData::KWinTouchScreenModuleData(QObject *parent)
    : KCModuleData(parent)
{
    m_deviceManager = new OrgKdeKWinInputDeviceManagerInterface(
        QStringLiteral("org.kde.KWin"),
        QStringLiteral("/org/kde/KWin/InputDevice"),
        QDBusConnection::sessionBus(),
        this);

    connect(m_deviceManager, &OrgKdeKWinInputDeviceManagerInterface::deviceAdded,
            this, &KWinTouchScreenModuleData::onDeviceAdded);
    connect(m_deviceManager, &OrgKdeKWinInputDeviceManagerInterface::deviceRemoved,
            this, &KWinTouchScreenModuleData::onDeviceRemoved);

    checkDevices();
}

void KWinTouchScreenModuleData::checkDevices()
{
    m_touchDevices = m_deviceManager->ListTouch();
    updateRelevance();
}

void KWinTouchScreenModuleData::onDeviceAdded()
{
    checkDevices();
}

void KWinTouchScreenModuleData::onDeviceRemoved()
{
    checkDevices();
}

void KWinTouchScreenModuleData::updateRelevance()
{
    setRelevant(!m_touchDevices.isEmpty());
}

}

#include "moc_kwintouchscreenmoduledata.cpp"

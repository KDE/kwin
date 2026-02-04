/*
    KWin - the KDE window manager
    This file is part of the KDE project.

    SPDX-FileCopyrightText: 2026 Alexander Wilms <f.alexander.wilms@outlook.com>

    SPDX-License-Identifier: GPL-2.0-or-later
*/

#pragma once

#include <KCModuleData>

class OrgKdeKWinInputDeviceManagerInterface;

namespace KWin
{

class KWinTouchScreenModuleData : public KCModuleData
{
    Q_OBJECT

public:
    explicit KWinTouchScreenModuleData(QObject *parent = nullptr);

private Q_SLOTS:
    void onDeviceAdded();
    void onDeviceRemoved();

private:
    void updateRelevance();
    void checkDevices();

    OrgKdeKWinInputDeviceManagerInterface *m_deviceManager = nullptr;
    QStringList m_touchDevices;
};

}

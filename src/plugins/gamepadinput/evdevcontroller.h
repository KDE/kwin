/*
 *   SPDX-FileCopyrightText: 2022 Aleix Pol i Gonzalez <aleixpol@kde.org>
 *
 *   SPDX-License-Identifier: GPL-2.0-only OR GPL-3.0-only OR LicenseRef-KDE-Accepted-GPL
 */

#pragma once

#include <Solid/Device>

#include "evdevdevice.h"
#include "gamepadinput.h"
#include "gaminginput_v2_interface.h"

namespace KWin
{

class EvdevDevice;
class EvdevController : public QObject
{
    Q_OBJECT
public:
    explicit EvdevController(QObject *parent);
    void init();
    bool addDevice(const Solid::Device &device);

Q_SIGNALS:
    void deviceAdded(EvdevDevice *device);
};
}

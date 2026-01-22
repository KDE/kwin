/*
    KWin - the KDE window manager
    This file is part of the KDE project.

    SPDX-FileCopyrightText: 2025 Yelsin Sepulveda <yelsin.sepulveda@kdemail.net>

    SPDX-License-Identifier: GPL-2.0-or-later
*/

#pragma once

#include "gamecontroller.h"

#include "plugin.h"
#include "utils/filedescriptor.h"
#include "utils/udev.h"
#include <libevdev/libevdev.h>

#include <QSocketNotifier>

namespace KWin
{

class Udev;
class UdevMonitor;
class UdevDevice;

class GameControllerManager : public Plugin
{
    Q_OBJECT

public:
    GameControllerManager();
    ~GameControllerManager() override;

    void handleUdevEvent();
    void handleFdAccess();

private:
    GameController *addGameController(const QString &devNode);
    bool removeGameController(const QString &devNode);
    bool isGameControllerDevice(libevdev *evdev);
    bool isTracked(const QString &path) const;
    bool isHandledByLibinput(const QString &devNode);

    bool addDeviceWatch(GameController *gc);
    void removeDeviceWatch(const QString &devNode);
    bool checkIfDeviceGrabbed(const QString &devicePath);

    std::unique_ptr<Udev> m_udev;
    std::unique_ptr<UdevMonitor> m_udevMonitor;
    std::unique_ptr<QSocketNotifier> m_socketNotifier;
    std::vector<std::unique_ptr<GameController>> m_gameControllers;

    FileDescriptor m_inotifyFd;
    FileDescriptor m_udevFd;
    std::unique_ptr<QSocketNotifier> m_inotifyNotifier;
    QMap<QString, int> m_deviceWatches;
    QHash<int, GameController *> m_watchesToGameControllers;
};

} // namespace KWin

/*
    KWin - the KDE window manager
    This file is part of the KDE project.

    SPDX-FileCopyrightText: 2025 Yelsin Sepulveda <yelsin.sepulveda@kdemail.net>

    SPDX-License-Identifier: GPL-2.0-or-later
*/

#pragma once

#include "emulatedinputdevice.h"
#include "utils/filedescriptor.h"

#include <libevdev/libevdev.h>

#include <QSocketNotifier>
#include <QString>

namespace KWin
{

struct LibevdevDeleter
{
    void operator()(libevdev *dev) const;
};

class GameController : public QObject
{
    Q_OBJECT

public:
    GameController(const QString &path, FileDescriptor &&fd, libevdev *evdev);
    ~GameController();

    const QString &path() const;
    int fd() const;
    libevdev *evdev() const;

    void increaseUsageCount();
    void decreaseUsageCount();
    int usageCount();

private:
    void handleEvdevEvent();
    void logEvent(input_event *ev);
    void enableInputEmulation();

    bool isTestEnvironment = qEnvironmentVariableIsSet("KWIN_GAMECONTROLLER_TEST");

    const QString m_path;
    FileDescriptor m_fd;
    int m_usageCount = 0;
    std::unique_ptr<libevdev, LibevdevDeleter> m_evdev;
    std::unique_ptr<EmulatedInputDevice> m_inputdevice;
    std::unique_ptr<QSocketNotifier> m_notifier;
};

} // namespace KWin

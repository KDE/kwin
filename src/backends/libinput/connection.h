/*
    KWin - the KDE window manager
    This file is part of the KDE project.

    SPDX-FileCopyrightText: 2014 Martin Gräßlin <mgraesslin@kde.org>

    SPDX-License-Identifier: GPL-2.0-or-later
*/
#pragma once

#include "effect/globals.h"

#include <KSharedConfig>

#include <QList>
#include <QObject>
#include <QPointer>
#include <QRecursiveMutex>
#include <QSize>
#include <QStringList>
#include <deque>

class QSocketNotifier;
class QThread;

struct libinput_tablet_tool;

namespace KWin
{

class Session;
class Udev;

namespace LibInput
{

class Event;
class Device;
class Context;
class ConnectionAdaptor;
class TabletTool;

class KWIN_EXPORT Connection : public QObject
{
    Q_OBJECT

public:
    ~Connection() override;

    void setInputConfig(const KSharedConfigPtr &config)
    {
        m_config = config;
    }

    void setup();
    void updateScreens();
    void deactivate();
    void processEvents();

    QStringList devicesSysNames() const;
    QStringList devicesSysNamesByKind(const QString &kind) const;

    static Connection *create(Session *session);

Q_SIGNALS:
    void deviceAdded(KWin::LibInput::Device *);
    void deviceRemoved(KWin::LibInput::Device *);

    void eventsRead();

private Q_SLOTS:
    void slotKGlobalSettingsNotifyChange(int type, int arg);

private:
    Connection(std::unique_ptr<Context> &&input);
    void handleEvent();
    void applyDeviceConfig(Device *device);
    void applyScreenToDevice(Device *device);
    void doSetup();
    TabletTool *getOrCreateTool(libinput_tablet_tool *tool);

    std::unique_ptr<QSocketNotifier> m_notifier;
    QRecursiveMutex m_mutex;
    std::deque<std::unique_ptr<Event>> m_eventQueue;
    QList<Device *> m_devices;
    QList<TabletTool *> m_tools;
    KSharedConfigPtr m_config;
    std::unique_ptr<ConnectionAdaptor> m_connectionAdaptor;
    std::unique_ptr<Context> m_input;
    std::unique_ptr<Udev> m_udev;
};

}
}

/*
    KWin - the KDE window manager
    This file is part of the KDE project.

    SPDX-FileCopyrightText: 2014 Martin Gräßlin <mgraesslin@kde.org>

    SPDX-License-Identifier: GPL-2.0-or-later
*/
#pragma once
#include <kwin_export.h>
#include <memory>

#include <QVector>
#include <vector>

#include <sys/types.h>

struct udev;
struct udev_device;
struct udev_monitor;

namespace KWin
{
class Udev;

class KWIN_EXPORT UdevDevice
{
public:
    UdevDevice(udev_device *device);
    ~UdevDevice();

    QString devNode() const;
    dev_t devNum() const;
    const char *property(const char *key);
    bool hasProperty(const char *key, const char *value);
    QString action() const;

    QMap<QByteArray, QByteArray> properties() const;
    bool isBootVga() const;
    QString seat() const;

    operator udev_device *() const
    {
        return m_device;
    }
    operator udev_device *()
    {
        return m_device;
    }
    typedef std::unique_ptr<UdevDevice> Ptr;

private:
    udev_device *const m_device;
};

class KWIN_EXPORT UdevMonitor
{
public:
    explicit UdevMonitor(Udev *udev);
    ~UdevMonitor();

    int fd() const;
    bool isValid() const
    {
        return m_monitor != nullptr;
    }
    void filterSubsystemDevType(const char *subSystem, const char *devType = nullptr);
    void enable();
    UdevDevice::Ptr getDevice();

private:
    udev_monitor *m_monitor;
};

class KWIN_EXPORT Udev
{
public:
    Udev();
    ~Udev();

    bool isValid() const
    {
        return m_udev != nullptr;
    }
    std::vector<UdevDevice::Ptr> listGPUs();
    UdevDevice::Ptr deviceFromSyspath(const char *syspath);
    std::unique_ptr<UdevMonitor> monitor();
    operator udev *() const
    {
        return m_udev;
    }
    operator udev *()
    {
        return m_udev;
    }

private:
    struct udev *m_udev;
};

}

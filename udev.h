/*
    KWin - the KDE window manager
    This file is part of the KDE project.

    SPDX-FileCopyrightText: 2014 Martin Gräßlin <mgraesslin@kde.org>

    SPDX-License-Identifier: GPL-2.0-or-later
*/
#ifndef KWIN_UDEV_H
#define KWIN_UDEV_H
#include <memory>
#include <kwin_export.h>

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

    udev_device *getParentWithSubsystemDevType(const char *subsystem, const char *devtype = nullptr) const;
    const char *devNode();
    int sysNum() const;
    const char *property(const char *key);
    bool hasProperty(const char *key, const char *value);

    operator udev_device*() const {
        return m_device;
    }
    operator udev_device*() {
        return m_device;
    }
    typedef std::unique_ptr<UdevDevice> Ptr;

private:
    udev_device *m_device;
};

class KWIN_EXPORT UdevMonitor
{
public:
    explicit UdevMonitor(Udev *udev);
    ~UdevMonitor();

    int fd() const;
    bool isValid() const {
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

    bool isValid() const {
        return m_udev != nullptr;
    }
    UdevDevice::Ptr primaryGpu();
    UdevDevice::Ptr primaryFramebuffer();
    UdevDevice::Ptr deviceFromSyspath(const char *syspath);
    UdevMonitor *monitor();
    operator udev*() const {
        return m_udev;
    }
    operator udev*() {
        return m_udev;
    }
private:
    struct udev *m_udev;
};

}

#endif

/********************************************************************
 KWin - the KDE window manager
 This file is part of the KDE project.

Copyright (C) 2014 Martin Gräßlin <mgraesslin@kde.org>

This program is free software; you can redistribute it and/or modify
it under the terms of the GNU General Public License as published by
the Free Software Foundation; either version 2 of the License, or
(at your option) any later version.

This program is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
GNU General Public License for more details.

You should have received a copy of the GNU General Public License
along with this program.  If not, see <http://www.gnu.org/licenses/>.
*********************************************************************/
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

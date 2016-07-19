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
#include "udev.h"
// Qt
#include <QByteArray>
#include <QScopedPointer>
// system
#include <libudev.h>
#include <functional>

namespace KWin
{

Udev::Udev()
    : m_udev(udev_new())
{
}

Udev::~Udev()
{
    if (m_udev) {
        udev_unref(m_udev);
    }
}

class UdevEnumerate
{
public:
    UdevEnumerate(Udev *udev);
    ~UdevEnumerate();

    enum class Match {
        SubSystem,
        SysName
    };
    void addMatch(Match match, const char *name);
    void scan();
    UdevDevice::Ptr find(std::function<bool(const UdevDevice::Ptr &)> test);

private:
    Udev *m_udev;

    struct EnumerateDeleter
    {
        static inline void cleanup(udev_enumerate *e)
        {
            udev_enumerate_unref(e);
        }
    };
    QScopedPointer<udev_enumerate, EnumerateDeleter> m_enumerate;
};

UdevEnumerate::UdevEnumerate(Udev *udev)
    : m_udev(udev)
    , m_enumerate(udev_enumerate_new(*m_udev))
{
}

UdevEnumerate::~UdevEnumerate() = default;

void UdevEnumerate::addMatch(UdevEnumerate::Match match, const char *name)
{
    if (m_enumerate.isNull()) {
        return;
    }
    switch (match) {
    case Match::SubSystem:
        udev_enumerate_add_match_subsystem(m_enumerate.data(), name);
        break;
    case Match::SysName:
        udev_enumerate_add_match_sysname(m_enumerate.data(), name);
        break;
    default:
        Q_UNREACHABLE();
        break;
    }
}

void UdevEnumerate::scan()
{
    if (m_enumerate.isNull()) {
        return;
    }
    udev_enumerate_scan_devices(m_enumerate.data());
}

UdevDevice::Ptr UdevEnumerate::find(std::function<bool(const UdevDevice::Ptr &device)> test)
{
    if (m_enumerate.isNull()) {
        return UdevDevice::Ptr();
    }
    udev_list_entry *it = udev_enumerate_get_list_entry(m_enumerate.data());
    UdevDevice::Ptr firstFound;
    while (it) {
        auto current = it;
        it = udev_list_entry_get_next(it);
        auto device = m_udev->deviceFromSyspath(udev_list_entry_get_name(current));
        if (!device) {
            continue;
        }
        if (test(device)) {
            return std::move(device);
        }
        if (!firstFound) {
            firstFound.swap(device);
        }
    }
    return std::move(firstFound);
}

UdevDevice::Ptr Udev::primaryGpu()
{
    if (!m_udev) {
        return UdevDevice::Ptr();
    }
    UdevEnumerate enumerate(this);
    enumerate.addMatch(UdevEnumerate::Match::SubSystem, "drm");
    enumerate.addMatch(UdevEnumerate::Match::SysName, "card[0-9]*");
    enumerate.scan();
    return enumerate.find([](const UdevDevice::Ptr &device) {
        // TODO: check seat
        auto pci = device->getParentWithSubsystemDevType("pci");
        if (!pci) {
            return false;
        }
        const char *systAttrValue = udev_device_get_sysattr_value(pci, "boot_vga");
        if (systAttrValue && qstrcmp(systAttrValue, "1") == 0) {
            return true;
        }
        return false;
    });
}

UdevDevice::Ptr Udev::virtualGpu()
{
    if (!m_udev) {
        return UdevDevice::Ptr();
    }
    UdevEnumerate enumerate(this);
    enumerate.addMatch(UdevEnumerate::Match::SubSystem, "drm");
    enumerate.addMatch(UdevEnumerate::Match::SysName, "card[0-9]*");
    enumerate.scan();
    return enumerate.find([](const UdevDevice::Ptr &device) {
        const QByteArray deviceName(udev_device_get_syspath(*device));
        return deviceName.contains("virtual");
    });
}

UdevDevice::Ptr Udev::renderNode()
{
    if (!m_udev) {
        return UdevDevice::Ptr();
    }
    UdevEnumerate enumerate(this);
    enumerate.addMatch(UdevEnumerate::Match::SubSystem, "drm");
    enumerate.addMatch(UdevEnumerate::Match::SysName, "renderD[0-9]*");
    enumerate.scan();
    return enumerate.find([](const UdevDevice::Ptr &device) {
        Q_UNUSED(device)
        return true;
    });
}

UdevDevice::Ptr Udev::deviceFromSyspath(const char *syspath)
{
    return std::move(UdevDevice::Ptr(new UdevDevice(udev_device_new_from_syspath(m_udev, syspath))));
}

UdevMonitor *Udev::monitor()
{
    UdevMonitor *m = new UdevMonitor(this);
    if (!m->isValid()) {
        delete m;
        m = nullptr;
    }
    return m;
}

UdevDevice::UdevDevice(udev_device *device)
    : m_device(device)
{
}

UdevDevice::~UdevDevice()
{
    if (m_device) {
        udev_device_unref(m_device);
    }
}

udev_device *UdevDevice::getParentWithSubsystemDevType(const char *subsystem, const char *devtype) const
{
    if (!m_device) {
        return nullptr;
    }
    return udev_device_get_parent_with_subsystem_devtype(m_device, subsystem, devtype);
}

const char *UdevDevice::devNode()
{
    if (!m_device) {
        return nullptr;
    }
    return udev_device_get_devnode(m_device);
}

int UdevDevice::sysNum() const
{
    if (!m_device) {
        return 0;
    }
    return QByteArray(udev_device_get_sysnum(m_device)).toInt();
}

const char *UdevDevice::property(const char *key)
{
    if (!m_device) {
        return nullptr;
    }
    return udev_device_get_property_value(m_device, key);
}

bool UdevDevice::hasProperty(const char *key, const char *value)
{
    const char *p = property(key);
    if (!p) {
        return false;
    }
    return qstrcmp(p, value) == 0;
}

UdevMonitor::UdevMonitor(Udev *udev)
    : m_udev(udev)
    , m_monitor(udev_monitor_new_from_netlink(*udev, "udev"))
{
}

UdevMonitor::~UdevMonitor()
{
    if (m_monitor) {
        udev_monitor_unref(m_monitor);
    }
}

int UdevMonitor::fd() const
{
    if (m_monitor) {
        return udev_monitor_get_fd(m_monitor);
    }
    return -1;
}

void UdevMonitor::filterSubsystemDevType(const char *subSystem, const char *devType)
{
    if (!m_monitor) {
        return;
    }
    udev_monitor_filter_add_match_subsystem_devtype(m_monitor, subSystem, devType);
}

void UdevMonitor::enable()
{
    if (!m_monitor) {
        return;
    }
    udev_monitor_enable_receiving(m_monitor);
}

UdevDevice::Ptr UdevMonitor::getDevice()
{
    if (!m_monitor) {
        return std::move(UdevDevice::Ptr());
    }
    return std::move(UdevDevice::Ptr(new UdevDevice(udev_monitor_receive_device(m_monitor))));
}

}

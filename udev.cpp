/*
    KWin - the KDE window manager
    This file is part of the KDE project.

    SPDX-FileCopyrightText: 2014 Martin Gräßlin <mgraesslin@kde.org>

    SPDX-License-Identifier: GPL-2.0-or-later
*/
#include "udev.h"
#include "logind.h"
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
    std::vector<UdevDevice::Ptr> find();

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

std::vector<UdevDevice::Ptr> UdevEnumerate::find()
{
    std::vector<UdevDevice::Ptr> vect;
    if (m_enumerate.isNull()) {
        vect.push_back( UdevDevice::Ptr() );
        return vect;
    }
    QString defaultSeat = QStringLiteral("seat0");
    udev_list_entry *it = udev_enumerate_get_list_entry(m_enumerate.data());
    while (it) {
        auto current = it;
        it = udev_list_entry_get_next(it);
        auto device = m_udev->deviceFromSyspath(udev_list_entry_get_name(current));
        if (!device) {
            continue;
        }
        QString deviceSeat = device->property("ID_SEAT");
        if (deviceSeat.isEmpty()) {
            deviceSeat = defaultSeat;
        }
        if (deviceSeat != LogindIntegration::self()->seat()) {
            continue;
        }
        if (device->getParentWithSubsystemDevType("pci"))
            vect.push_back(std::move(device));
    }
    return vect;
}

std::vector<UdevDevice::Ptr> Udev::listGPUs()
{
    if (!m_udev) {
        std::vector<UdevDevice::Ptr> vect;
        vect.push_back(UdevDevice::Ptr());
        return vect;
    }
#if defined(Q_OS_FREEBSD)
    std::vector<UdevDevice::Ptr> r;
    r.push_back(deviceFromSyspath("/dev/dri/card0"));
    return r;
#else
    UdevEnumerate enumerate(this);
    enumerate.addMatch(UdevEnumerate::Match::SubSystem, "drm");
    enumerate.addMatch(UdevEnumerate::Match::SysName, "card[0-9]");
    enumerate.scan();
    auto vect = enumerate.find();
    std::sort(vect.begin(), vect.end(), [](const UdevDevice::Ptr &device1, const UdevDevice::Ptr &device2) {
        auto pci1 = device1->getParentWithSubsystemDevType("pci");
        auto pci2 = device2->getParentWithSubsystemDevType("pci");
        // if set as boot GPU, prefer 1
        const char *systAttrValue = udev_device_get_sysattr_value(pci1, "boot_vga");
        if (systAttrValue && qstrcmp(systAttrValue, "1") == 0) {
            return true;
        }
        // if set as boot GPU, prefer 2
        systAttrValue = udev_device_get_sysattr_value(pci2, "boot_vga");
        if (systAttrValue && qstrcmp(systAttrValue, "1") == 0) {
            return false;
        }
        return udev_device_get_sysnum(pci1) > udev_device_get_sysnum(pci2);
    });
    return vect;
#endif
}

std::vector<UdevDevice::Ptr> Udev::listFramebuffers()
{
    if (!m_udev) {
        std::vector<UdevDevice::Ptr> vect;
        vect.push_back(UdevDevice::Ptr());
        return vect;
    }
    UdevEnumerate enumerate(this);
    enumerate.addMatch(UdevEnumerate::Match::SubSystem, "graphics");
    enumerate.addMatch(UdevEnumerate::Match::SysName, "fb[0-9]");
    enumerate.scan();
    auto vect = enumerate.find();
    std::sort(vect.begin(), vect.end(), [](const UdevDevice::Ptr &device1, const UdevDevice::Ptr &device2) {
        auto pci1 = device1->getParentWithSubsystemDevType("pci");
        auto pci2 = device2->getParentWithSubsystemDevType("pci");
        // if set as boot GPU, prefer 1
        const char *systAttrValue = udev_device_get_sysattr_value(pci1, "boot_vga");
        if (systAttrValue && qstrcmp(systAttrValue, "1") == 0) {
            return true;
        }
        // if set as boot GPU, prefer 2
        systAttrValue = udev_device_get_sysattr_value(pci2, "boot_vga");
        if (systAttrValue && qstrcmp(systAttrValue, "1") == 0) {
            return false;
        }
        return udev_device_get_sysnum(pci1) > udev_device_get_sysnum(pci2);
    });
    return vect;
}

UdevDevice::Ptr Udev::deviceFromSyspath(const char *syspath)
{
    return UdevDevice::Ptr(new UdevDevice(udev_device_new_from_syspath(m_udev, syspath)));
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
    : m_monitor(udev_monitor_new_from_netlink(*udev, "udev"))
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
        return UdevDevice::Ptr();
    }
    return UdevDevice::Ptr(new UdevDevice(udev_monitor_receive_device(m_monitor)));
}

}

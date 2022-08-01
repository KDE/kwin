/*
    KWin - the KDE window manager
    This file is part of the KDE project.

    SPDX-FileCopyrightText: 2014 Martin Gräßlin <mgraesslin@kde.org>

    SPDX-License-Identifier: GPL-2.0-or-later
*/
#include "udev.h"
#include "utils/common.h"
// Qt
#include <QByteArray>
#include <QDebug>
// system
#include <cerrno>
#include <functional>
#include <libudev.h>

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
        void operator()(udev_enumerate *e)
        {
            udev_enumerate_unref(e);
        }
    };
    std::unique_ptr<udev_enumerate, EnumerateDeleter> m_enumerate;
};

UdevEnumerate::UdevEnumerate(Udev *udev)
    : m_udev(udev)
    , m_enumerate(udev_enumerate_new(*m_udev))
{
}

UdevEnumerate::~UdevEnumerate() = default;

void UdevEnumerate::addMatch(UdevEnumerate::Match match, const char *name)
{
    if (!m_enumerate) {
        return;
    }
    switch (match) {
    case Match::SubSystem:
        udev_enumerate_add_match_subsystem(m_enumerate.get(), name);
        break;
    case Match::SysName:
        udev_enumerate_add_match_sysname(m_enumerate.get(), name);
        break;
    default:
        Q_UNREACHABLE();
        break;
    }
}

void UdevEnumerate::scan()
{
    if (!m_enumerate) {
        return;
    }
    udev_enumerate_scan_devices(m_enumerate.get());
}

std::vector<UdevDevice::Ptr> UdevEnumerate::find()
{
    if (!m_enumerate) {
        return {};
    }
    std::vector<UdevDevice::Ptr> vect;
    udev_list_entry *it = udev_enumerate_get_list_entry(m_enumerate.get());
    while (it) {
        auto current = it;
        it = udev_list_entry_get_next(it);
        auto device = m_udev->deviceFromSyspath(udev_list_entry_get_name(current));
        if (device) {
            vect.push_back(std::move(device));
        }
    }
    return vect;
}

std::vector<UdevDevice::Ptr> Udev::listGPUs()
{
    if (!m_udev) {
        return {};
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
        // if set as boot GPU, prefer 1
        if (device1->isBootVga()) {
            return true;
        }
        // if set as boot GPU, prefer 2
        if (device2->isBootVga()) {
            return false;
        }
        return true;
    });
    return vect;
#endif
}

UdevDevice::Ptr Udev::deviceFromSyspath(const char *syspath)
{
    auto dev = udev_device_new_from_syspath(m_udev, syspath);
    if (!dev) {
        qCWarning(KWIN_CORE) << "failed to retrieve device for" << syspath << strerror(errno);
        return {};
    }
    return UdevDevice::Ptr(new UdevDevice(dev));
}

std::unique_ptr<UdevMonitor> Udev::monitor()
{
    auto m = std::make_unique<UdevMonitor>(this);
    if (m->isValid()) {
        return m;
    } else {
        return nullptr;
    }
}

UdevDevice::UdevDevice(udev_device *device)
    : m_device(device)
{
    Q_ASSERT(device);
}

UdevDevice::~UdevDevice()
{
    udev_device_unref(m_device);
}

QString UdevDevice::devNode() const
{
    return QString::fromUtf8(udev_device_get_devnode(m_device));
}

dev_t UdevDevice::devNum() const
{
    return udev_device_get_devnum(m_device);
}

const char *UdevDevice::property(const char *key)
{
    return udev_device_get_property_value(m_device, key);
}

QMap<QByteArray, QByteArray> UdevDevice::properties() const
{
    QMap<QByteArray, QByteArray> r;
    auto it = udev_device_get_properties_list_entry(m_device);
    auto current = it;
    udev_list_entry_foreach(current, it)
    {
        r.insert(udev_list_entry_get_name(current), udev_list_entry_get_value(current));
    }
    return r;
}

bool UdevDevice::hasProperty(const char *key, const char *value)
{
    const char *p = property(key);
    if (!p) {
        return false;
    }
    return qstrcmp(p, value) == 0;
}

bool UdevDevice::isBootVga() const
{
    auto pci = udev_device_get_parent_with_subsystem_devtype(m_device, "pci", nullptr);
    if (!pci) {
        return false;
    }
    const char *systAttrValue = udev_device_get_sysattr_value(pci, "boot_vga");
    return systAttrValue && qstrcmp(systAttrValue, "1") == 0;
}

QString UdevDevice::seat() const
{
    QString deviceSeat = udev_device_get_property_value(m_device, "ID_SEAT");
    if (deviceSeat.isEmpty()) {
        deviceSeat = QStringLiteral("seat0");
    }
    return deviceSeat;
}

QString UdevDevice::action() const
{
    return QString::fromLocal8Bit(udev_device_get_action(m_device));
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
    auto dev = udev_monitor_receive_device(m_monitor);
    if (!dev) {
        return {};
    }

    return UdevDevice::Ptr(new UdevDevice(dev));
}

}

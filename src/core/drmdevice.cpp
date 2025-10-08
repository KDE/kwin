/*
    KWin - the KDE window manager
    This file is part of the KDE project.

    SPDX-FileCopyrightText: 2024 Xaver Hugl <xaver.hugl@gmail.com>

    SPDX-License-Identifier: GPL-2.0-or-later
*/
#include "drmdevice.h"

#include "gbmgraphicsbufferallocator.h"
#include "utils/common.h"

#include <fcntl.h>
#include <gbm.h>
#include <sys/stat.h>
#include <xf86drm.h>

namespace KWin
{

DrmDevice::DrmDevice(const QString &path, dev_t id, FileDescriptor &&fd, gbm_device *gbmDevice, drmDevice *libdrmDevice)
    : m_path(path)
    , m_id(id)
    , m_fd(std::move(fd))
    , m_gbmDevice(gbmDevice)
    , m_allocator(std::make_unique<GbmGraphicsBufferAllocator>(this))
    , m_libdrmDevice(libdrmDevice)
{
    uint64_t value = 0;
    m_supportsSyncObjTimelines = drmGetCap(m_fd.get(), DRM_CAP_SYNCOBJ_TIMELINE, &value) == 0 && value != 0;
}

DrmDevice::~DrmDevice()
{
    gbm_device_destroy(m_gbmDevice);
    drmFreeDevice(&m_libdrmDevice);
}

QString DrmDevice::path() const
{
    return m_path;
}

dev_t DrmDevice::deviceId() const
{
    return m_id;
}

gbm_device *DrmDevice::gbmDevice() const
{
    return m_gbmDevice;
}

GraphicsBufferAllocator *DrmDevice::allocator() const
{
    return m_allocator.get();
}

int DrmDevice::fileDescriptor() const
{
    return m_fd.get();
}

bool DrmDevice::supportsSyncObjTimelines() const
{
    return m_supportsSyncObjTimelines;
}

std::optional<drmPciDeviceInfo> DrmDevice::pciDeviceInfo() const
{
    drmDevice *nativeDevice = nullptr;
    auto nativeDeviceCleanup = qScopeGuard([&nativeDevice]() {
        drmFreeDevice(&nativeDevice);
    });

    if (drmGetDeviceFromDevId(deviceId(), 0, &nativeDevice) != 0) {
        return std::nullopt;
    }

    if (nativeDevice->bustype != DRM_BUS_PCI) {
        return std::nullopt;
    }

    return *nativeDevice->deviceinfo.pci;
}

bool DrmDevice::isSameDevice(DrmDevice *other) const
{
    return other == this || drmDevicesEqual(m_libdrmDevice, other->m_libdrmDevice);
}

drmDevice *DrmDevice::libdrmDevice() const
{
    return m_libdrmDevice;
}

std::shared_ptr<DrmDevice> DrmDevice::open(const QString &path)
{
    return openWithAuthentication(path, -1);
}

std::shared_ptr<DrmDevice> DrmDevice::openWithAuthentication(const QString &path, int authenticatedFd)
{
    FileDescriptor fd(::open(path.toLocal8Bit(), O_RDWR | O_CLOEXEC));
    if (!fd.isValid()) {
        qCWarning(KWIN_CORE) << "Failed to open drm node:" << path;
        return nullptr;
    }
    struct stat buf;
    if (fstat(fd.get(), &buf) == -1) {
        qCWarning(KWIN_CORE) << "Failed to fstat drm fd" << path;
        return nullptr;
    }
    if (authenticatedFd != -1) {
        drm_magic_t magic;
        if (drmGetMagic(fd.get(), &magic) < 0) {
            qCDebug(KWIN_CORE) << "Failed to get the drm magic token for" << path;
        }
        if (drmAuthMagic(authenticatedFd, magic) < 0) {
            qCWarning(KWIN_CORE) << "Failed to authenticate the drm magic token. path:" << path << "error:" << strerror(errno);
        }
    }
    drmDevice *nativeDevice = nullptr;
    if (drmGetDeviceFromDevId(buf.st_rdev, 0, &nativeDevice) != 0) {
        return nullptr;
    }
    gbm_device *gbmDevice = gbm_create_device(fd.get());
    if (!gbmDevice) {
        qCWarning(KWIN_CORE) << "Failed to create gbm device for" << path;
        drmFreeDevice(&nativeDevice);
        return nullptr;
    }
    return std::make_shared<DrmDevice>(path, buf.st_rdev, std::move(fd), gbmDevice, nativeDevice);
}
}

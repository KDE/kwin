/*
    KWin - the KDE window manager
    This file is part of the KDE project.

    SPDX-FileCopyrightText: 2024 Xaver Hugl <xaver.hugl@gmail.com>

    SPDX-License-Identifier: GPL-2.0-or-later
*/
#pragma once

#include "graphicsbufferallocator.h"
#include "kwin_export.h"
#include "utils/filedescriptor.h"

#include <QString>

#include <memory>
#include <sys/types.h>
#include <xf86drm.h>

struct gbm_device;

namespace KWin
{

class KWIN_EXPORT DrmDevice : public std::enable_shared_from_this<DrmDevice>
{
public:
    explicit DrmDevice(const QString &path, dev_t id, FileDescriptor &&fd, gbm_device *gbmDevice, drmDevice *libdrmDevice);
    ~DrmDevice();

    QString path() const;
    dev_t deviceId() const;
    gbm_device *gbmDevice() const;
    GraphicsBufferAllocator *allocator() const;
    int fileDescriptor() const;
    bool supportsSyncObjTimelines() const;
    std::optional<drmPciDeviceInfo> pciDeviceInfo() const;
    drmDevice *libdrmDevice() const;

    bool isSameDevice(DrmDevice *other) const;

    static std::shared_ptr<DrmDevice> open(const QString &path);
    static std::shared_ptr<DrmDevice> openWithAuthentication(const QString &path, int authenticatedFd);

private:
    const QString m_path;
    const dev_t m_id;
    const FileDescriptor m_fd;
    gbm_device *const m_gbmDevice;
    const std::unique_ptr<GraphicsBufferAllocator> m_allocator;
    bool m_supportsSyncObjTimelines;
    drmDevice *m_libdrmDevice = nullptr;
};

}

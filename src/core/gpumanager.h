/*
    KWin - the KDE window manager
    This file is part of the KDE project.

    SPDX-FileCopyrightText: 2026 Xaver Hugl <xaver.huglkde.org>

    SPDX-License-Identifier: GPL-2.0-or-later
*/
#pragma once
#include "core/graphicsbuffer.h"
#include "kwin_export.h"
#include "renderdevice.h"
#include "utils/filedescriptor.h"

#include <sys/types.h>
#include <xf86drm.h>

class QSocketNotifier;

namespace KWin
{

class Udev;
class UdevMonitor;
class DrmDevice;

class KWIN_EXPORT GpuManager : public QObject
{
    Q_OBJECT
public:
    static std::unique_ptr<GpuManager> s_self;
    static GpuManager *self();

    explicit GpuManager();
    ~GpuManager();

    const std::vector<std::unique_ptr<RenderDevice>> &renderDevices() const;
    RenderDevice *compatibleRenderDevice(DrmDevice *dev) const;
    RenderDevice *compatibleRenderDevice(dev_t id) const;
    RenderDevice *findDevice(dev_t id) const;
    /**
     * @returns a software renderdevice backed by udmabuf (and no drm device)
     */
    RenderDevice *softwareDevice() const;

    /**
     * NOTE that the list is automatically updated through
     * udev, there is usually no need to call this.
     */
    void scanForRenderDevices();

    /**
     * Creates a udmabuf based on the attributes. This import will fail
     * if the offset or size aren't page-aligned!
     * NOTE this doesn't do any copies, the returned dmabuf references
     *      the exact same memory as the shm buffer. Synchronization of
     *      both CPU and GPU side accesses is up to the importer!
     */
    std::optional<DmaBufAttributes> createUdmabuf(const ShmAttributes *attributes) const;
    const FileDescriptor &udmabuf() const;

    void addDevice(std::unique_ptr<RenderDevice> &&kmsSoftwareDevice);
    void removeDevice(RenderDevice *kmsSoftwareDevice);

Q_SIGNALS:
    void renderDeviceAdded(RenderDevice *device);
    void renderDeviceRemoved(RenderDevice *device);

private:
    void handleUdevEvent();
    void updateCompatibilityMap();
    RenderDevice *findCompatibleRenderDevice(drmDevicePtr device) const;

    const FileDescriptor m_udmabuf;
    const std::optional<dev_t> m_udmabufDevId;
    const std::unique_ptr<Udev> m_udev;
    const std::unique_ptr<UdevMonitor> m_udevMonitor;
    const std::unique_ptr<QSocketNotifier> m_udevNotifier;
    RenderDevice *m_softwareDevice = nullptr;
    std::vector<std::unique_ptr<RenderDevice>> m_renderDevices;
    QHash<dev_t, RenderDevice *> m_compatibleDeviceMap;
};

}

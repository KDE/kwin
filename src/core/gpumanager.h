/*
    KWin - the KDE window manager
    This file is part of the KDE project.

    SPDX-FileCopyrightText: 2026 Xaver Hugl <xaver.huglkde.org>

    SPDX-License-Identifier: GPL-2.0-or-later
*/
#pragma once
#include "kwin_export.h"
#include "renderdevice.h"

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

    explicit GpuManager();
    ~GpuManager();

    const std::vector<std::unique_ptr<RenderDevice>> &renderDevices() const;
    RenderDevice *compatibleRenderDevice(DrmDevice *dev) const;
    RenderDevice *findDevice(dev_t id) const;

    /**
     * NOTE that the list is automatically updated through
     * udev, there is usually no need to call this.
     */
    void scanForRenderDevices();

Q_SIGNALS:
    void renderDeviceAdded(RenderDevice *device);
    void renderDeviceRemoved(RenderDevice *device);

private:
    void handleUdevEvent();

    const std::unique_ptr<Udev> m_udev;
    const std::unique_ptr<UdevMonitor> m_udevMonitor;
    const std::unique_ptr<QSocketNotifier> m_udevNotifier;
    std::vector<std::unique_ptr<RenderDevice>> m_renderDevices;
};

}

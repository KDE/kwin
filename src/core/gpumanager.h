#pragma once
#include "drmdevice.h"
#include "kwin_export.h"
#include "opengl/egldisplay.h"

#include <QObject>

class QSocketNotifier;

namespace KWin
{

class Udev;
class UdevMonitor;

struct RenderDevice
{
    std::shared_ptr<DrmDevice> device;
    std::unique_ptr<EglDisplay> eglDisplay;
};

class KWIN_EXPORT GpuManager : public QObject
{
    Q_OBJECT
public:
    explicit GpuManager();
    ~GpuManager();

    QList<RenderDevice *> renderDevices() const;
    /**
     * @returns a device that can render to buffers allocated
     *          on the scanout device. May be nullptr!
     */
    RenderDevice *findCompatibleDevice(DrmDevice *scanoutDevice) const;
    RenderDevice *findDevice(DrmDevice *renderDevice) const;

Q_SIGNALS:
    void deviceAdded(RenderDevice *gpu);
    void deviceRemoved(RenderDevice *gpu);

private:
    void addDevice(const QString &devNode);
    void removeDevice(RenderDevice *device);
    void handleUdevEvent();

    std::unique_ptr<Udev> m_udev;
    std::unique_ptr<UdevMonitor> m_udevMonitor;
    std::unique_ptr<QSocketNotifier> m_socketNotifier;
    std::vector<std::unique_ptr<RenderDevice>> m_renderDevices;
};

}

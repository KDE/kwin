#include "gpumanager.h"
#include "utils/udev.h"

#include <QDebug>
#include <QList>
#include <QSocketNotifier>

namespace KWin
{

GpuManager::GpuManager()
    : m_udev(std::make_unique<Udev>())
    , m_udevMonitor(std::make_unique<UdevMonitor>(m_udev.get()))
{
    const auto devs = m_udev->listRenderDevices();
    for (const auto &dev : devs) {
        addDevice(dev->devNode());
    }
    m_udevMonitor->filterSubsystemDevType("drm");
    m_udevMonitor->enable();
    m_socketNotifier = std::make_unique<QSocketNotifier>(m_udevMonitor->fd(), QSocketNotifier::Read);
    connect(m_socketNotifier.get(), &QSocketNotifier::activated, this, &GpuManager::handleUdevEvent);
}

GpuManager::~GpuManager()
{
}

void GpuManager::handleUdevEvent()
{
    const auto find = [this](dev_t id) {
        const auto it = std::ranges::find_if(m_renderDevices, [id](const auto &dev) {
            return dev->device->deviceId() == id;
        });
        return it == m_renderDevices.end() ? nullptr : it->get();
    };
    while (auto device = m_udevMonitor->getDevice()) {
        if (device->action() == QLatin1StringView("add")) {
            auto dev = find(device->devNum());
            if (dev) {
                continue;
            }
            addDevice(device->devNode());
        } else if (device->action() == QLatin1StringView("remove")) {
            auto dev = find(device->devNum());
            if (!dev) {
                continue;
            }
            removeDevice(dev);
        }
    }
}

void GpuManager::addDevice(const QString &devNode)
{
    // TODO filter KMS nodes out with udev directly?
    if (devNode.contains("card")) {
        return;
    }
    auto dev = DrmDevice::open(devNode);
    if (!dev) {
        return;
    }
    qWarning() << "GPUMAN added device" << devNode;
    m_renderDevices.push_back(std::make_unique<RenderDevice>(RenderDevice{
        .device = std::move(dev),
        .eglDisplay = nullptr,
    }));
    Q_EMIT deviceAdded(m_renderDevices.back().get());
}

void GpuManager::removeDevice(RenderDevice *device)
{
    const auto it = std::ranges::find_if(m_renderDevices, [device](const auto &other) {
        return other.get() == device;
    });
    if (it != m_renderDevices.end()) {
        auto tmp = std::move(*it);
        qWarning() << "GPUMAN removed device" << device->device->path();
        Q_EMIT deviceRemoved(tmp.get());
        m_renderDevices.erase(it);
    }
}

QList<RenderDevice *> GpuManager::renderDevices() const
{
    return m_renderDevices | std::views::transform(&std::unique_ptr<RenderDevice>::get) | std::ranges::to<QList>();
}

RenderDevice *GpuManager::findCompatibleDevice(DrmDevice *scanoutDevice) const
{
    if (m_renderDevices.empty()) {
        return nullptr;
    }
    const auto sameDeviceIt = std::ranges::find_if(m_renderDevices, [scanoutDevice](const auto &renderDevice) {
        return scanoutDevice->isSameDevice(renderDevice->device.get());
    });
    if (sameDeviceIt != m_renderDevices.end()) {
        return sameDeviceIt->get();
    }
    // next best choice: platform to platform or faux is efficient
    if (scanoutDevice->libdrmDevice()->bustype == DRM_BUS_PLATFORM
        || scanoutDevice->libdrmDevice()->bustype == DRM_BUS_FAUX) {
        const auto it = std::ranges::find_if(m_renderDevices, [](const auto &renderDevice) {
            return renderDevice->device->libdrmDevice()->bustype == DRM_BUS_PLATFORM
                || renderDevice->device->libdrmDevice()->bustype == DRM_BUS_FAUX;
        });
        if (it != m_renderDevices.end()) {
            return it->get();
        }
    }
    // fallback: if the device in question is kms-only, just pick the first GPU
    if (scanoutDevice->libdrmDevice()->available_nodes == DRM_NODE_PRIMARY) {
        return m_renderDevices.front().get();
    }
    return nullptr;
}

RenderDevice *GpuManager::findDevice(DrmDevice *renderDevice) const
{
    const auto it = std::ranges::find_if(m_renderDevices, [renderDevice](const auto &dev) {
        return dev->device->isSameDevice(renderDevice);
    });
    return it == m_renderDevices.end() ? nullptr : it->get();
}
}

#include "moc_gpumanager.cpp"

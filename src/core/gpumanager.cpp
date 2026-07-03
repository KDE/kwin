/*
    KWin - the KDE window manager
    This file is part of the KDE project.

    SPDX-FileCopyrightText: 2026 Xaver Hugl <xaver.huglkde.org>

    SPDX-License-Identifier: GPL-2.0-or-later
*/
#include "gpumanager.h"
#include "drmdevice.h"
#include "opengl/egldisplay.h"
#include "utils/common.h"
#include "utils/envvar.h"
#include "utils/udev.h"
#include "vulkan/vulkan_device.h"

#include <QSocketNotifier>
#include <fcntl.h>
#include <sys/ioctl.h>
#include <sys/stat.h>
#include <unistd.h>

#if defined(Q_OS_LINUX)
#include <linux/udmabuf.h>
#endif

namespace KWin
{

std::unique_ptr<GpuManager> GpuManager::s_self;

GpuManager *GpuManager::self()
{
    return s_self.get();
}

static std::optional<dev_t> getDevId(const FileDescriptor &fd)
{
    if (!fd.isValid()) {
        return std::nullopt;
    }
    struct stat buf;
    if (fstat(fd.get(), &buf) == -1) {
        qCWarning(KWIN_CORE) << "Failed to fstat udmabuf fd";
        return std::nullopt;
    }
    return buf.st_rdev;
}

GpuManager::GpuManager()
    : m_udmabuf(::open("/dev/udmabuf", O_RDWR | O_CLOEXEC))
    , m_udmabufDevId(getDevId(m_udmabuf))
    , m_udev(std::make_unique<Udev>())
    , m_udevMonitor(m_udev->createMonitor())
    , m_udevNotifier(std::make_unique<QSocketNotifier>(m_udevMonitor->fd(), QSocketNotifier::Read))
{
    m_udevMonitor->filterSubsystemDevType("drm");
    connect(m_udevNotifier.get(), &QSocketNotifier::activated, this, &GpuManager::handleUdevEvent);
    m_udevMonitor->enable();
    scanForRenderDevices();
}

GpuManager::~GpuManager()
{
    m_renderDevices.clear();
}

const std::vector<std::unique_ptr<RenderDevice>> &GpuManager::renderDevices() const
{
    return m_renderDevices;
}

RenderDevice *GpuManager::compatibleRenderDevice(DrmDevice *dev) const
{
    return compatibleRenderDevice(dev->deviceId());
}

RenderDevice *GpuManager::compatibleRenderDevice(dev_t id) const
{
    auto it = m_compatibleDeviceMap.find(id);
    return it == m_compatibleDeviceMap.end() ? nullptr : it.value();
}

RenderDevice *GpuManager::findDevice(dev_t id) const
{
    const auto it = std::ranges::find_if(m_renderDevices, [id](const auto &device) {
        return device->drmDevice()->deviceId() == id;
    });
    return it == m_renderDevices.end() ? nullptr : it->get();
}

void GpuManager::updateCompatibilityMap()
{
    m_compatibleDeviceMap.clear();

    int numberOfDevices = drmGetDevices2(0, nullptr, 0);
    if (numberOfDevices <= 0) {
        return;
    }
    std::vector<drmDevicePtr> devices;
    devices.resize(numberOfDevices);
    if (drmGetDevices2(0, devices.data(), numberOfDevices) < 0) {
        qCWarning(KWIN_CORE, "drmGetDevices2 failed: %s", strerror(errno));
        return;
    }
    for (drmDevicePtr device : devices) {
        RenderDevice *render = findCompatibleRenderDevice(device);
        if (device->available_nodes & (1 << DRM_NODE_PRIMARY)) {
            struct stat buf;
            if (stat(device->nodes[DRM_NODE_PRIMARY], &buf) == -1) {
                qCWarning(KWIN_CORE) << "Failed to fstat drm fd" << device->nodes[DRM_NODE_PRIMARY];
            } else {
                m_compatibleDeviceMap[buf.st_rdev] = render;
            }
        }
        if (device->available_nodes & (1 << DRM_NODE_RENDER)) {
            struct stat buf;
            if (stat(device->nodes[DRM_NODE_RENDER], &buf) == -1) {
                qCWarning(KWIN_CORE) << "Failed to fstat drm fd" << device->nodes[DRM_NODE_RENDER];
            } else {
                m_compatibleDeviceMap[buf.st_rdev] = render;
            }
        }
    }
    drmFreeDevices(devices.data(), devices.size());
}

RenderDevice *GpuManager::findCompatibleRenderDevice(drmDevicePtr device) const
{
    auto candidates = m_renderDevices | std::views::filter([device](const auto &renderDevice) {
        if (device->bustype == DRM_BUS_PLATFORM) {
            // devices with bus "platform" can be assumed to be compatible.
            return renderDevice->drmDevice()->busType() == DRM_BUS_PLATFORM;
        } else {
            // "normal" GPUs are only compatible if there's a direct match
            return drmDevicesEqual(device, renderDevice->drmDevice()->libdrmDevice()) == 1;
        }
    });
    if (candidates.empty()) {
        return nullptr;
    }
    // With split display/render devices, there can be multiple render nodes,
    // and with "normal" GPUs we want to avoid using the fallback KMS RenderDevice
    return std::ranges::min_element(candidates, [device](const auto &left, const auto &right) {
        // most important: we want hardware acceleration
        if (left->eglDisplay()->isSoftwareRenderer() != right->eglDisplay()->isSoftwareRenderer()) {
            return !left->eglDisplay()->isSoftwareRenderer();
        }
        // if possible, we want Vulkan support as well
        if (bool(left->vulkanDevice()) != bool(right->vulkanDevice())) {
            return bool(left->vulkanDevice());
        }
        // (also with hardware acceleration)
        if (left->vulkanDevice() && right->vulkanDevice()
            && left->vulkanDevice()->isSoftwareRenderer() != right->vulkanDevice()->isSoftwareRenderer()) {
            return !left->vulkanDevice()->isSoftwareRenderer();
        }
        // if both have hardware acceleration, prefer a matching device
        const bool sameDeviceLeft = drmDevicesEqual(device, left->drmDevice()->libdrmDevice()) == 1;
        const bool sameDeviceRight = drmDevicesEqual(device, right->drmDevice()->libdrmDevice()) == 1;
        if (sameDeviceLeft != sameDeviceRight) {
            return sameDeviceLeft;
        }
        // prefer render nodes over KMS nodes
        if (left->drmDevice()->isKMS() != right->drmDevice()->isKMS()) {
            return !left->drmDevice()->isKMS();
        }
        // fallback: if both are equally good,
        // make sure we always get the same device
        return left->drmDevice()->deviceId() < right->drmDevice()->deviceId();
    })->get();
}

void GpuManager::scanForRenderDevices()
{
    const auto devices = m_udev->listRenderNodes();
    for (const auto &udevDevice : devices) {
        if (findDevice(udevDevice->devNum())) {
            continue;
        }
        auto device = RenderDevice::open(udevDevice->devNode());
        if (!device) {
            continue;
        }
        qCDebug(KWIN_CORE, "Adding render device %s", qPrintable(device->drmDevice()->path()));
        m_renderDevices.push_back(std::move(device));
        updateCompatibilityMap();
        Q_EMIT renderDeviceAdded(m_renderDevices.back().get());
    }
}

void GpuManager::handleUdevEvent()
{
    while (auto udevDevice = m_udevMonitor->getDevice()) {
        int devNum = -1;
        const bool isPrimaryNode = sscanf(udevDevice->sysName().data(), DRM_PRIMARY_MINOR_NAME "%d", &devNum) == 1;
        if (isPrimaryNode) {
            if (udevDevice->action() == QLatin1StringView("add") || udevDevice->action() == QLatin1StringView("remove")) {
                updateCompatibilityMap();
            }
            continue;
        }
        const bool isRenderNode = sscanf(udevDevice->sysName().data(), DRM_RENDER_MINOR_NAME "%d", &devNum) == 1;
        if (!isRenderNode) {
            continue;
        }
        const auto renderDevIt = std::ranges::find_if(m_renderDevices, [&udevDevice](const auto &device) {
            return udevDevice->devNum() == device->drmDevice()->deviceId();
        });
        if (udevDevice->action() == QLatin1StringView("add")) {
            if (renderDevIt != m_renderDevices.end()) {
                continue;
            }
            auto device = RenderDevice::open(udevDevice->devNode());
            if (!device) {
                continue;
            }
            addDevice(std::move(device));
        } else if (udevDevice->action() == QLatin1StringView("remove")) {
            if (renderDevIt == m_renderDevices.end()) {
                continue;
            }
            removeDevice(renderDevIt->get());
        }
    }
}

static uint64_t align(uint64_t size, uint64_t minimum)
{
    if (auto remainder = size % minimum) {
        return size + (minimum - remainder);
    } else {
        return size;
    }
}

std::optional<DmaBufAttributes> GpuManager::createUdmabuf(const ShmAttributes *attributes) const
{
#if defined(Q_OS_LINUX)
    if (!m_udmabuf.isValid() || !m_udmabufDevId) {
        return std::nullopt;
    }
    if (attributes->offset % getpagesize() != 0) {
        return std::nullopt;
    }
    struct udmabuf_create create{
        .memfd = uint32_t(attributes->fd.get()),
        .flags = UDMABUF_FLAGS_CLOEXEC,
        .offset = uint64_t(attributes->offset),
        .size = align(attributes->size.height() * attributes->stride, getpagesize()),
    };
    int dmabufFd = ioctl(m_udmabuf.get(), UDMABUF_CREATE, &create);
    if (dmabufFd < 0) {
        qCDebug(KWIN_CORE, "Could not create udmabuf: %s", strerror(errno));
        return std::nullopt;
    }

    DmaBufAttributes ret;
    ret.planeCount = 1;
    ret.width = attributes->size.width();
    ret.height = attributes->size.height();
    ret.format = attributes->format;
    ret.modifier = DRM_FORMAT_MOD_LINEAR;
    // NOTE this may require special handling in the future,
    // since in theory udmabuf can be imported into any device
    ret.device = *m_udmabufDevId;
    ret.fd[0] = FileDescriptor{dmabufFd};
    ret.offset[0] = 0;
    ret.pitch[0] = attributes->stride;
    return ret;
#else
    return std::nullopt;
#endif
}

const FileDescriptor &GpuManager::udmabuf() const
{
    return m_udmabuf;
}

void GpuManager::addDevice(std::unique_ptr<RenderDevice> &&device)
{
    m_renderDevices.push_back(std::move(device));
    updateCompatibilityMap();
    qCDebug(KWIN_CORE, "Added render device %s", qPrintable(m_renderDevices.back()->drmDevice()->path()));
    Q_EMIT renderDeviceAdded(m_renderDevices.back().get());
}

void GpuManager::removeDevice(RenderDevice *device)
{
    const auto it = std::ranges::find_if(m_renderDevices, [device](const auto &dev) {
        return dev.get() == device;
    });
    if (it == m_renderDevices.end()) {
        return;
    }
    auto ref = std::move(*it);
    m_renderDevices.erase(it);
    updateCompatibilityMap();
    qCDebug(KWIN_CORE, "Removed render device %s", qPrintable(device->drmDevice()->path()));
    Q_EMIT renderDeviceRemoved(device);
}

}

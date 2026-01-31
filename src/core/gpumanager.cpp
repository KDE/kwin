/*
    KWin - the KDE window manager
    This file is part of the KDE project.

    SPDX-FileCopyrightText: 2026 Xaver Hugl <xaver.huglkde.org>

    SPDX-License-Identifier: GPL-2.0-or-later
*/
#include "gpumanager.h"
#include "drmdevice.h"
#include "utils/common.h"
#include "utils/udev.h"

#include <QSocketNotifier>
#include <vulkan/vulkan.hpp>

// This takes care of loading extension functions
// as long as the vulkan-hpp versions of the functions are used
VULKAN_HPP_DEFAULT_DISPATCH_LOADER_DYNAMIC_STORAGE

namespace KWin
{

std::unique_ptr<GpuManager> GpuManager::s_self;

GpuManager::GpuManager()
    : m_udev(std::make_unique<Udev>())
    , m_udevMonitor(m_udev->createMonitor())
    , m_udevNotifier(std::make_unique<QSocketNotifier>(m_udevMonitor->fd(), QSocketNotifier::Read))
{
    m_udevMonitor->filterSubsystemDevType("drm");
    connect(m_udevNotifier.get(), &QSocketNotifier::activated, this, &GpuManager::handleUdevEvent);
    m_udevMonitor->enable();
    initVulkan();
    scanForRenderDevices();
}

GpuManager::~GpuManager()
{
    m_renderDevices.clear();
    if (m_vulkanInstance) {
        vkDestroyInstance(m_vulkanInstance, nullptr);
    }
}

const std::vector<std::unique_ptr<RenderDevice>> &GpuManager::renderDevices() const
{
    return m_renderDevices;
}

RenderDevice *GpuManager::compatibleRenderDevice(DrmDevice *dev) const
{
    auto matchingIt = std::ranges::find_if(m_renderDevices, [dev](const auto &renderDevice) {
        return dev->equals(renderDevice->drmDevice());
    });
    if (matchingIt != m_renderDevices.end()) {
        return matchingIt->get();
    }
    // fallback for split display/render cases: devices with bus "platform" can be assumed to be compatible
    if (dev->busType() == DRM_BUS_PLATFORM) {
        matchingIt = std::ranges::find_if(m_renderDevices, [](const auto &renderDevice) {
            return renderDevice->drmDevice()->busType() == DRM_BUS_PLATFORM;
        });
        if (matchingIt != m_renderDevices.end()) {
            return matchingIt->get();
        }
    }
    return nullptr;
}

RenderDevice *GpuManager::compatibleRenderDevice(dev_t id) const
{
    // TODO monitor KMS nodes on the system, and maintain a dev_t -> RenderDevice *
    // hash map, instead of looking this stuff up every time...
    if (RenderDevice *ret = findDevice(id)) {
        return ret;
    }
    drmDevice *device = nullptr;
    if (drmGetDeviceFromDevId(id, 0, &device) != 0) {
        return nullptr;
    }
    auto matchingIt = std::ranges::find_if(m_renderDevices, [device](const auto &renderDevice) {
        return drmDevicesEqual(device, renderDevice->drmDevice()->libdrmDevice()) == 0;
    });
    if (matchingIt != m_renderDevices.end()) {
        return matchingIt->get();
    }
    // fallback for split display/render cases: devices with bus "platform" can be assumed to be compatible
    if (device->bustype == DRM_BUS_PLATFORM) {
        matchingIt = std::ranges::find_if(m_renderDevices, [](const auto &renderDevice) {
            return renderDevice->drmDevice()->busType() == DRM_BUS_PLATFORM;
        });
        if (matchingIt != m_renderDevices.end()) {
            return matchingIt->get();
        }
    }
    return nullptr;
}

RenderDevice *GpuManager::findDevice(dev_t id) const
{
    const auto it = std::ranges::find_if(m_renderDevices, [id](const auto &device) {
        return device->drmDevice()->deviceId() == id;
    });
    return it == m_renderDevices.end() ? nullptr : it->get();
}

void GpuManager::scanForRenderDevices()
{
    const auto devices = m_udev->listRenderNodes();
    for (const auto &udevDevice : devices) {
        if (findDevice(udevDevice->devNum())) {
            continue;
        }
        auto device = RenderDevice::open(udevDevice->devNode(), m_vulkanInstance);
        if (!device) {
            continue;
        }
        qCDebug(KWIN_CORE, "Adding render device %s", qPrintable(device->drmDevice()->path()));
        m_renderDevices.push_back(std::move(device));
    }
}

void GpuManager::initVulkan()
{
    // this makes createInstance work
    VULKAN_HPP_DEFAULT_DISPATCHER.init();
    vk::ApplicationInfo appInfo{
        "kwin_wayland",
        // TODO use defines for this
        VK_MAKE_VERSION(6, 7, 0),
        "kwin_wayland",
        VK_MAKE_VERSION(1, 0, 0),
        VK_MAKE_VERSION(1, 3, 0),
    };
    std::vector<const char *> validationLayers;
    validationLayers.push_back("VK_LAYER_KHRONOS_validation");
    const vk::InstanceCreateInfo instanceCI{
        vk::InstanceCreateFlags(),
        &appInfo,
        validationLayers,
    };
    const auto [result, instance] = vk::createInstance(instanceCI, nullptr);
    if (result != vk::Result::eSuccess) {
        qCWarning(KWIN_CORE) << "couldn't create a vulkan instance:" << vk::to_string(result).c_str();
        return;
    }
    m_vulkanInstance = instance;
    // this loads all other function pointers for us
    VULKAN_HPP_DEFAULT_DISPATCHER.init(instance);
}

VkInstance GpuManager::vulkanInstance() const
{
    return m_vulkanInstance;
}

void GpuManager::handleUdevEvent()
{
    while (auto udevDevice = m_udevMonitor->getDevice()) {
        if (!udevDevice->devNode().contains("renderD")) {
            // not a render node, should be ignored
            continue;
        }
        const auto renderDevIt = std::ranges::find_if(m_renderDevices, [&udevDevice](const auto &device) {
            return udevDevice->devNum() == device->drmDevice()->deviceId();
        });
        if (udevDevice->action() == QLatin1StringView("add")) {
            if (renderDevIt != m_renderDevices.end()) {
                continue;
            }
            auto device = RenderDevice::open(udevDevice->devNode(), m_vulkanInstance);
            if (!device) {
                continue;
            }
            m_renderDevices.push_back(std::move(device));
            qCDebug(KWIN_CORE, "Added render device %s", qPrintable(m_renderDevices.back()->drmDevice()->path()));
            Q_EMIT renderDeviceAdded(m_renderDevices.back().get());
        } else if (udevDevice->action() == QLatin1StringView("remove")) {
            if (renderDevIt == m_renderDevices.end()) {
                continue;
            }
            auto device = std::move(*renderDevIt);
            m_renderDevices.erase(renderDevIt);
            qCDebug(KWIN_CORE, "Removed render device %s", qPrintable(device->drmDevice()->path()));
            Q_EMIT renderDeviceRemoved(device.get());
        }
    }
}

}

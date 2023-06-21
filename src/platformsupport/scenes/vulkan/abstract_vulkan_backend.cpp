/*
    KWin - the KDE window manager
    This file is part of the KDE project.

    SPDX-FileCopyrightText: 2023 Xaver Hugl <xaver.hugl@gmail.com>

    SPDX-License-Identifier: GPL-2.0-or-later
*/
#include "abstract_vulkan_backend.h"

#include "core/gbmgraphicsbufferallocator.h"
#include "core/graphicsbuffer.h"
#include "utils/drm_format_helper.h"
#include "vulkan_device.h"
#include "vulkan_texture.h"

#include <sys/stat.h>
#include <sys/sysmacros.h>

namespace KWin
{

AbstractVulkanBackend::AbstractVulkanBackend() = default;
AbstractVulkanBackend::~AbstractVulkanBackend() = default;

bool AbstractVulkanBackend::init()
{
    vk::ApplicationInfo appInfo{
        "kwin_wayland",
        VK_MAKE_VERSION(6, 0, 0),
        "kwin_wayland",
        VK_MAKE_VERSION(1, 0, 0),
        VK_MAKE_VERSION(1, 3, 0)};
    // TODO only enable the validation layer if it's installed, creating the instance fails otherwise
    // also, versions lower than 1.3.250 crash when importing dmabufs
    // -> maybe make an env var for this option
    std::array validationLayers{"VK_LAYER_KHRONOS_validation"};
    const vk::InstanceCreateInfo instanceCI(
        vk::InstanceCreateFlags(),
        &appInfo,
        validationLayers);
    if (auto ret = vk::createInstance(instanceCI); ret.result == vk::Result::eSuccess) {
        m_instance = ret.value;
    } else {
        qWarning() << "couldn't create a vulkan instance:" << vk::to_string(ret.result).c_str();
        return false;
    }
    auto [physicalDeviceResult, physicalDevices] = m_instance.enumeratePhysicalDevices();
    if (physicalDeviceResult != vk::Result::eSuccess) {
        qWarning() << "querying vulkan devices failed:" << vk::to_string(physicalDeviceResult);
        return false;
    }
    if (physicalDevices.empty()) {
        qWarning() << "couldn't find a vulkan device";
        return false;
    }
    for (auto it = physicalDevices.begin(); it != physicalDevices.end(); it++) {
        const auto physicalDevice = *it;
        const auto props1 = physicalDevice.getProperties();
        const auto deviceName = props1.deviceName.data();
        if (const auto extensionProps = physicalDevice.enumerateDeviceExtensionProperties(); extensionProps.result == vk::Result::eSuccess) {
            const auto supportedExtensions = extensionProps.value;
            const std::array required = {
                VK_EXT_PHYSICAL_DEVICE_DRM_EXTENSION_NAME,
                VK_KHR_EXTERNAL_MEMORY_FD_EXTENSION_NAME,
                VK_EXT_EXTERNAL_MEMORY_DMA_BUF_EXTENSION_NAME,
                VK_EXT_IMAGE_DRM_FORMAT_MODIFIER_EXTENSION_NAME,
            };
            std::vector missing(required.begin(), required.end());
            std::erase_if(missing, [&supportedExtensions](const char *req) {
                return std::any_of(supportedExtensions.begin(), supportedExtensions.end(), [req](const auto &ext) {
                    return strcmp(req, ext.extensionName) == 0;
                });
            });
            if (!missing.empty()) {
                qWarning() << "Missing required extensions";
                for (const char *str : missing) {
                    qWarning() << str;
                }
                qWarning() << "Ignoring device" << deviceName;
                continue;
            }
        } else {
            qWarning() << "failed to query extension properties, ignoring device" << deviceName;
            continue;
        }
        vk::PhysicalDeviceDrmPropertiesEXT drmProps;
        vk::PhysicalDeviceProperties2 props2(props1, &drmProps);
        physicalDevice.getProperties2(&props2);
        qWarning() << "found vulkan device:" << deviceName << "primary?" << drmProps.hasPrimary << ", render?" << drmProps.hasRender << "" << drmProps.primaryMajor << "." << drmProps.primaryMinor << ", " << drmProps.renderMajor << "." << drmProps.renderMinor;

        std::optional<dev_t> primaryNode;
        std::optional<dev_t> renderNode;
        if (drmProps.hasPrimary) {
            primaryNode = makedev(drmProps.primaryMajor, drmProps.primaryMinor);
        }
        if (drmProps.hasRender) {
            renderNode = makedev(drmProps.renderMajor, drmProps.renderMinor);
        }
        if (!primaryNode && !renderNode) {
            qWarning() << "Vulkan device has neither primary nor render node, ignoring device" << deviceName;
            continue;
        }

        const auto queueFamilyProperties = physicalDevice.getQueueFamilyProperties();
        const auto queueFamilyIt = std::find_if(queueFamilyProperties.begin(), queueFamilyProperties.end(), [](const vk::QueueFamilyProperties &properties) {
            return properties.queueFlags & (vk::QueueFlagBits::eGraphics);
        });
        if (queueFamilyIt == queueFamilyProperties.end()) {
            qWarning() << "failed to find a queue famlity with graphis and transfer capabilities. Ignoring device" << deviceName;
            continue;
        }
        const uint32_t queueFamilyIndex = std::distance(queueFamilyProperties.begin(), queueFamilyIt);

        float priority = 1.0f;
        std::array queueCreateInfos = {
            vk::DeviceQueueCreateInfo{
                vk::DeviceQueueCreateFlags(),
                queueFamilyIndex,
                1,
                &priority,
                nullptr},
        };
        std::vector<const char *> enabledLayerNames = {};
        std::vector<const char *> enabledExtensionNames = {
            VK_KHR_EXTERNAL_MEMORY_FD_EXTENSION_NAME,
            VK_EXT_EXTERNAL_MEMORY_DMA_BUF_EXTENSION_NAME,
            VK_EXT_IMAGE_DRM_FORMAT_MODIFIER_EXTENSION_NAME,
        };
        vk::PhysicalDeviceFeatures enabledFeatures;
        vk::PhysicalDeviceDynamicRenderingFeatures dynamicRendering(true);

        vk::DeviceCreateInfo deviceCI{
            vk::DeviceCreateFlags(),
            queueCreateInfos,
            enabledLayerNames,
            enabledExtensionNames,
            &enabledFeatures,
            &dynamicRendering};
        const auto [deviceResult, logicalDevice] = physicalDevice.createDevice(deviceCI);
        if (deviceResult != vk::Result::eSuccess) {
            qWarning() << "failed to create a logical device. Ignoring device" << deviceName;
            continue;
        }

        VulkanDevice device(
            m_instance,
            physicalDevice,
            logicalDevice,
            queueFamilyIndex,
            logicalDevice.getQueue(queueFamilyIndex, 0),
            primaryNode,
            renderNode);
        if (device.supportedFormats().isEmpty()) {
            continue;
        }
        m_devices.push_back(std::make_unique<VulkanDevice>(std::move(device)));
    }
    return true;
}

CompositingType AbstractVulkanBackend::compositingType() const
{
    return CompositingType::VulkanCompositing;
}

OutputLayer *AbstractVulkanBackend::primaryLayer(Output *output)
{
    return nullptr;
}

void AbstractVulkanBackend::present(Output *output)
{
}

}

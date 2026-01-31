/*
    KWin - the KDE window manager
    This file is part of the KDE project.

    SPDX-FileCopyrightText: 2026 Xaver Hugl <xaver.hugl@kde.org>

    SPDX-License-Identifier: GPL-2.0-or-later
*/
#include "renderdevice.h"

#include "drmdevice.h"
#include "graphicsbuffer.h"
#include "opengl/eglcontext.h"
#include "opengl/egldisplay.h"
#include "utils/common.h"
#include "vulkan/vulkan_device.h"
#include "vulkan/vulkan_logging.h"

#include <expected>
#include <sys/sysmacros.h>
#include <vulkan/vulkan.hpp>
#include <vulkan/vulkan_raii.hpp>

namespace KWin
{

RenderDevice::RenderDevice(std::unique_ptr<DrmDevice> &&device, std::unique_ptr<EglDisplay> &&display, std::unique_ptr<VulkanDevice> &&vulkanDevice)
    : m_device(std::move(device))
    , m_display(std::move(display))
    , m_vulkanDevice(std::move(vulkanDevice))
{
}

RenderDevice::~RenderDevice()
{
    for (const auto &image : m_importedBuffers) {
        m_display->destroyImage(image);
    }
}

DrmDevice *RenderDevice::drmDevice() const
{
    return m_device.get();
}

EglDisplay *RenderDevice::eglDisplay() const
{
    return m_display.get();
}

std::shared_ptr<EglContext> RenderDevice::eglContext(EglContext *shareContext)
{
    auto ret = m_eglContext.lock();
    if (!ret || ret->isFailed()) {
        ret = EglContext::create(m_display.get(), EGL_NO_CONFIG_KHR, shareContext ? shareContext->handle() : EGL_NO_CONTEXT);
        m_eglContext = ret;
    }
    return ret;
}

EGLImageKHR RenderDevice::importBufferAsImage(GraphicsBuffer *buffer)
{
    std::pair key(buffer, 0);
    auto it = m_importedBuffers.constFind(key);
    if (Q_LIKELY(it != m_importedBuffers.constEnd())) {
        return *it;
    }

    Q_ASSERT(buffer->dmabufAttributes());
    EGLImageKHR image = m_display->importDmaBufAsImage(*buffer->dmabufAttributes());
    if (image != EGL_NO_IMAGE_KHR) {
        m_importedBuffers[key] = image;
        connect(buffer, &QObject::destroyed, this, [this, key]() {
            m_display->destroyImage(m_importedBuffers.take(key));
        });
        return image;
    } else {
        qCWarning(KWIN_OPENGL) << "failed to import dmabuf" << buffer;
        return EGL_NO_IMAGE;
    }
}

EGLImageKHR RenderDevice::importBufferAsImage(GraphicsBuffer *buffer, int plane, int format, const QSize &size)
{
    std::pair key(buffer, plane);
    auto it = m_importedBuffers.constFind(key);
    if (Q_LIKELY(it != m_importedBuffers.constEnd())) {
        return *it;
    }

    Q_ASSERT(buffer->dmabufAttributes());
    EGLImageKHR image = m_display->importDmaBufAsImage(*buffer->dmabufAttributes(), plane, format, size);
    if (image != EGL_NO_IMAGE_KHR) {
        m_importedBuffers[key] = image;
        connect(buffer, &QObject::destroyed, this, [this, key]() {
            m_display->destroyImage(m_importedBuffers.take(key));
        });
        return image;
    } else {
        qCWarning(KWIN_OPENGL) << "failed to import dmabuf" << buffer;
        return EGL_NO_IMAGE;
    }
}

VulkanDevice *RenderDevice::vulkanDevice() const
{
    return m_vulkanDevice.get();
}

static constexpr std::array s_requiredVulkanExtensions = {
    // allows getting the dev_t of each VkPhysicalDevice
    VK_EXT_PHYSICAL_DEVICE_DRM_EXTENSION_NAME,
    // allow importing dma-bufs
    VK_KHR_EXTERNAL_MEMORY_FD_EXTENSION_NAME,
    VK_EXT_EXTERNAL_MEMORY_DMA_BUF_EXTENSION_NAME,
    VK_EXT_IMAGE_DRM_FORMAT_MODIFIER_EXTENSION_NAME,
    // allows importing and exporting sync fds
    VK_KHR_EXTERNAL_FENCE_FD_EXTENSION_NAME,
    // allows using the "general" image layout nearly everywhere
    VK_KHR_UNIFIED_IMAGE_LAYOUTS_EXTENSION_NAME,
};

static std::unique_ptr<VulkanDevice> openVulkanDevice(VkInstance instance, DrmDevice *drm)
{
    const auto [enumerateResult, physicalDevices] = vk::Instance(instance).enumeratePhysicalDevices();
    if (enumerateResult != vk::Result::eSuccess) {
        qCWarning(KWIN_VULKAN) << "querying vulkan devices failed:" << vk::to_string(enumerateResult);
        return nullptr;
    }
    for (const vk::PhysicalDevice &physicalDevice : physicalDevices) {
        const auto [extensionPropResult, extensionProps] = physicalDevice.enumerateDeviceExtensionProperties();
        if (extensionPropResult != vk::Result::eSuccess) {
            continue;
        }
        std::vector missingExtensions = s_requiredVulkanExtensions | std::ranges::to<std::vector>();
        std::erase_if(missingExtensions, [&extensionProps](std::string_view required) {
            return std::ranges::any_of(extensionProps, [required](const auto &ext) {
                return required == ext.extensionName;
            });
        });
        if (!missingExtensions.empty()) {
            qCWarning(KWIN_VULKAN, "Device misses required Vulkan extensions");
            for (const char *str : missingExtensions) {
                qCWarning(KWIN_VULKAN) << str;
            }
            continue;
        }
        const auto fenceProperties = physicalDevice.getExternalFenceProperties(vk::PhysicalDeviceExternalFenceInfo{
            vk::ExternalFenceHandleTypeFlagBits::eSyncFd,
        });
        if (!(fenceProperties.externalFenceFeatures & vk::ExternalFenceFeatureFlagBits::eExportable)) {
            qCWarning(KWIN_VULKAN, "Vulkan device can't export sync fds");
            continue;
        }

        vk::PhysicalDeviceDrmPropertiesEXT drmProps;
        vk::PhysicalDeviceProperties2 properties({}, &drmProps);
        physicalDevice.getProperties2(&properties);
        if (!drmProps.hasRender) {
            qCDebug(KWIN_VULKAN, "Skipping device without a render node");
            continue;
        }
        if (drm->deviceId() != makedev(drmProps.renderMajor, drmProps.renderMinor)) {
            continue;
        }
        std::vector<vk::QueueFamilyProperties> queueProperties = physicalDevice.getQueueFamilyProperties();
        const bool hasTransfer = std::ranges::any_of(queueProperties, [](const vk::QueueFamilyProperties &props) {
            return bool(props.queueFlags & vk::QueueFlagBits::eTransfer);
        });
        if (!hasTransfer) {
            qCWarning(KWIN_VULKAN, "Physical device has no transfer queue");
            continue;
        }

        std::vector<VkDeviceQueueCreateInfo> queueCI;
        float priority = 1;
        for (uint32_t i = 0; i < queueProperties.size(); i++) {
            queueCI.push_back(VkDeviceQueueCreateInfo{
                .sType = VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO,
                .pNext = nullptr,
                .flags = {},
                .queueFamilyIndex = i,
                .queueCount = 1,
                .pQueuePriorities = &priority,
            });
        }

        VkPhysicalDeviceFeatures features{
            .robustBufferAccess = true,
        };
        VkDeviceCreateInfo deviceCI{
            .sType = VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO,
            .pNext = nullptr,
            .flags = {},
            .queueCreateInfoCount = uint32_t(queueCI.size()),
            .pQueueCreateInfos = queueCI.data(),
            .enabledLayerCount = 0,
            .ppEnabledLayerNames = nullptr,
            .enabledExtensionCount = s_requiredVulkanExtensions.size(),
            .ppEnabledExtensionNames = s_requiredVulkanExtensions.data(),
            .pEnabledFeatures = &features,
        };

        VkDevice logicalDevice;
        VkResult result = vkCreateDevice(physicalDevice, &deviceCI, nullptr, &logicalDevice);
        if (result != VK_SUCCESS) {
            qCWarning(KWIN_VULKAN, "vkCreateDevice failed: %s", vk::to_string(vk::Result(result)).c_str());
            continue;
        }

        VulkanDevice device{
            instance,
            physicalDevice,
            logicalDevice,
            queueProperties | std::ranges::to<std::vector<VkQueueFamilyProperties>>(),
        };
        if (device.supportedFormats().isEmpty()) {
            continue;
        }
        qCDebug(KWIN_VULKAN, "Found Vulkan device %s for %s", properties.properties.deviceName.data(), qPrintable(drm->path()));
        return std::make_unique<VulkanDevice>(std::move(device));
    }
    qCDebug(KWIN_VULKAN, "No Vulkan device found for %s", qPrintable(drm->path()));
    return nullptr;
}

std::unique_ptr<RenderDevice> RenderDevice::open(const QString &path, VkInstance vkInstance, int authenticatedFd)
{
    auto drmDevice = DrmDevice::openWithAuthentication(path, authenticatedFd);
    if (!drmDevice) {
        return nullptr;
    }
    auto eglDisplay = EglDisplay::create(eglGetPlatformDisplayEXT(EGL_PLATFORM_GBM_KHR, drmDevice->gbmDevice(), nullptr));
    if (!eglDisplay) {
        return nullptr;
    }
    std::unique_ptr<VulkanDevice> vulkanDevice;
    if (vkInstance) {
        vulkanDevice = openVulkanDevice(vkInstance, drmDevice.get());
    }
    return std::make_unique<RenderDevice>(std::move(drmDevice), std::move(eglDisplay), std::move(vulkanDevice));
}

}

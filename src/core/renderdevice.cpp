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
#include "utils/envvar.h"
#include "vulkan/vulkan_device.h"
#include "vulkan/vulkan_logging.h"

#include <expected>
#include <vulkan/vulkan_raii.hpp>
#if __has_include(<sys/sysmacros.h>)
#include <sys/sysmacros.h>
#endif

namespace KWin
{

static FormatModifierMap getImportFormats(EglDisplay *eglDisplay, VulkanDevice *vulkanDevice)
{
    FormatModifierMap ret;
    if (eglDisplay) {
        ret = eglDisplay->allSupportedDrmFormats();
    }
    if (vulkanDevice) {
        ret = ret.merged(vulkanDevice->supportedFormats());
    }
    return ret;
}

// NOTE that we have to create an instance per render device, as Mesa
// only updates the list of devices on the first vkEnumeratePhysicalDevices
// call per VkInstance!
static vk::raii::Instance createVulkanInstance(const vk::raii::Context &context)
{
    vk::ApplicationInfo appInfo{
        "kwin_wayland",
        VK_MAKE_VERSION(PROJECT_VERSION_MAJOR, PROJECT_VERSION_MINOR, PROJECT_VERSION_PATCH),
        "kwin_wayland",
        VK_MAKE_VERSION(1, 0, 0),
        VK_MAKE_VERSION(1, 3, 0),
    };
    std::vector<const char *> validationLayers;
    if (environmentVariableBoolValue("KWIN_VULKAN_VALIDATION").value_or(PROJECT_VERSION_PATCH >= 80)) {
        validationLayers.push_back("VK_LAYER_KHRONOS_validation");
    }
    vk::InstanceCreateInfo instanceCI{
        vk::InstanceCreateFlags(),
        &appInfo,
        validationLayers,
    };
    auto [result, instance] = context.createInstance(instanceCI);
    // auto [result, instance] = vk::raii::createInstance(instanceCI, nullptr);
    if (result != vk::Result::eSuccess && !validationLayers.empty()) {
        // try again without the validation layer
        validationLayers.clear();
        instanceCI.setPEnabledLayerNames(validationLayers);
        auto [result, instance] = context.createInstance(instanceCI);
        if (result == vk::Result::eSuccess) {
            qCWarning(KWIN_CORE, "Vulkan validation layer is not installed");
            return std::move(instance);
        }
    }
    return std::move(instance);
}

RenderDevice::RenderDevice(std::unique_ptr<DrmDevice> &&device, std::unique_ptr<EglDisplay> &&display)
    : m_device(std::move(device))
    , m_display(std::move(display))
    , m_vulkanInstance(createVulkanInstance(m_vulkanContext))
{
    createVulkanDevice();
    m_allImportableFormats = getImportFormats(m_display.get(), m_vulkanDevice.get());
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

const FormatModifierMap &RenderDevice::allImportableFormats() const
{
    return m_allImportableFormats;
}

static constexpr std::array s_requiredVulkanExtensions = {
    // allows getting the dev_t of each VkPhysicalDevice
    VK_EXT_PHYSICAL_DEVICE_DRM_EXTENSION_NAME,
    // allow importing dma-bufs
    VK_KHR_EXTERNAL_MEMORY_FD_EXTENSION_NAME,
    VK_EXT_EXTERNAL_MEMORY_DMA_BUF_EXTENSION_NAME,
    VK_EXT_IMAGE_DRM_FORMAT_MODIFIER_EXTENSION_NAME,
    // allow importing and exporting sync fds
    VK_KHR_EXTERNAL_FENCE_FD_EXTENSION_NAME,
    VK_KHR_EXTERNAL_SEMAPHORE_FD_EXTENSION_NAME,
};

static std::unique_ptr<VulkanDevice> openVulkanDevice(const vk::raii::Instance &instance, DrmDevice *drm)
{
    const auto [enumerateResult, physicalDevices] = instance.enumeratePhysicalDevices();
    if (enumerateResult != vk::Result::eSuccess) {
        qCWarning(KWIN_VULKAN) << "querying vulkan devices failed:" << vk::to_string(enumerateResult);
        return nullptr;
    }
    for (const vk::raii::PhysicalDevice &physicalDevice : physicalDevices) {
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

        const auto propertiesChain = physicalDevice.getProperties2<vk::PhysicalDeviceProperties2, vk::PhysicalDeviceDrmPropertiesEXT>();
        const auto &drmProps = propertiesChain.get<vk::PhysicalDeviceDrmPropertiesEXT>();
        const auto &properties = propertiesChain.get<vk::PhysicalDeviceProperties2>();
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

        auto [result, logicalDevice] = physicalDevice.createDevice(deviceCI);
        if (result != vk::Result::eSuccess) {
            qCWarning(KWIN_VULKAN, "vkCreateDevice failed: %s", vk::to_string(vk::Result(result)).c_str());
            continue;
        }

        auto ret = std::make_unique<VulkanDevice>(
            physicalDevice,
            std::move(logicalDevice),
            queueProperties | std::ranges::to<std::vector<VkQueueFamilyProperties>>());
        if (ret->supportedFormats().isEmpty()) {
            continue;
        }
        qCWarning(KWIN_VULKAN, "Found Vulkan device %s for %s", properties.properties.deviceName.data(), qPrintable(drm->path()));
        return ret;
    }
    qCDebug(KWIN_VULKAN, "No Vulkan device found for %s", qPrintable(drm->path()));
    return nullptr;
}

void RenderDevice::handleVulkanDeviceLoss()
{
    m_inReset = true;
    // This is done with a queued connection to avoid deleting the Vulkan device
    // before other parts of KWin are able to clean up their Vulkan resources
    QMetaObject::invokeMethod(this, &RenderDevice::createVulkanDevice, Qt::QueuedConnection);
}

void RenderDevice::createVulkanDevice()
{
    m_vulkanDevice = openVulkanDevice(m_vulkanInstance, m_device.get());
    if (m_vulkanDevice) {
        connect(m_vulkanDevice.get(), &VulkanDevice::deviceLost, this, &RenderDevice::handleVulkanDeviceLoss);
    }
    m_inReset = false;
}

bool RenderDevice::isInReset() const
{
    return m_inReset;
}

std::unique_ptr<RenderDevice> RenderDevice::open(const QString &path, int authenticatedFd)
{
    auto drmDevice = DrmDevice::openWithAuthentication(path, authenticatedFd);
    if (!drmDevice) {
        return nullptr;
    }
    auto eglDisplay = EglDisplay::create(eglGetPlatformDisplayEXT(EGL_PLATFORM_GBM_KHR, drmDevice->gbmDevice(), nullptr));
    if (!eglDisplay) {
        return nullptr;
    }
    return std::make_unique<RenderDevice>(std::move(drmDevice), std::move(eglDisplay));
}

}

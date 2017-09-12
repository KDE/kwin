/********************************************************************
 KWin - the KDE window manager
 This file is part of the KDE project.

Copyright © 2017-2018 Fredrik Höglund <fredrik@kde.org>

This program is free software; you can redistribute it and/or modify
it under the terms of the GNU General Public License as published by
the Free Software Foundation; either version 2 of the License, or
(at your option) any later version.

This program is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
GNU General Public License for more details.

You should have received a copy of the GNU General Public License
along with this program.  If not, see <http://www.gnu.org/licenses/>.
*********************************************************************/

#include "scene.h"
#include "swapchain.h"
#include "window.h"
#include "shadow.h"
#include "decorationrenderer.h"
#include "descriptorset.h"
#include "utils.h"

#include "effectframe.h"
#include "platform.h"
#include "screens.h"
#include "logging.h"
#include "platformsupport/scenes/vulkan/backend.h"
#include "options.h"

#include <QGraphicsScale>
#include <KNotification>
#include <KLocalizedString>


using namespace std::placeholders;


namespace KWin {


VulkanFactory::VulkanFactory(QObject *parent)
    : SceneFactory(parent)
{
}


VulkanFactory::~VulkanFactory() = default;


Scene *VulkanFactory::create(QObject *parent) const
{
    auto backend = std::unique_ptr<VulkanBackend>(kwinApp()->platform()->createVulkanBackend());

    if (!backend || !backend->isValid())
        return nullptr;

    VulkanScene *scene = new VulkanScene(std::move(backend), parent);

    if (scene && scene->initFailed()) {
        delete scene;
        scene = nullptr;
    }

    return scene;
}



// ------------------------------------------------------------------



VulkanScene::VulkanScene(std::unique_ptr<VulkanBackend> &&backend, QObject *parent)
    : Scene(parent),
      m_backend(std::move(backend))
{
    init();
}


VulkanScene::~VulkanScene()
{
    if (!m_device)
        return;

    m_device->waitIdle();

    for (FrameData &data : m_frameData) {
        if (data.imageAcquisitionFenceSubmitted)
            data.imageAcquisitionFence.wait();
    }
}


bool VulkanScene::init()
{
    // Instance layers
    // ---------------
    std::vector<const char *> enabledLayers;

    if (qgetenv("KWIN_ENABLE_VULKAN_VALIDATION") == "1") {
        // Note that VK_LAYER_LUNARG_standard_validation is a meta layer that
        // loads all standard validation layers in the optimal order.
        // The loader removes duplicates, so it's safe to list the others anyway.
        static const char * const validationLayers[] {
            "VK_LAYER_LUNARG_standard_validation",
            "VK_LAYER_GOOGLE_threading",
            "VK_LAYER_LUNARG_parameter_validation",
            "VK_LAYER_LUNARG_object_tracker",
            "VK_LAYER_LUNARG_core_validation",
            "VK_LAYER_GOOGLE_unique_objects",
        };

        const VulkanLayerVector supportedLayers = VulkanInstance::supportedLayers();
        enabledLayers.reserve(std::extent<decltype(validationLayers)>());

        for (const char *layer : validationLayers) {
            if (supportedLayers.contains(layer)) {
                enabledLayers.push_back(layer);
            }
        }
    }

    // Instance extensions
    // -------------------
    const VulkanExtensionVector supportedInstanceExtensions = VulkanInstance::supportedExtensions();

    const char *requiredInstanceExtensions[] = {
        VK_KHR_SURFACE_EXTENSION_NAME,
        m_backend->platformSurfaceExtensionName()
    };

    std::vector<const char *> enabledInstanceExtensions;
    enabledInstanceExtensions.reserve(std::extent<decltype(requiredInstanceExtensions)>());

    for (auto extension : requiredInstanceExtensions) {
        if (!supportedInstanceExtensions.contains(extension)) {
            qCCritical(KWIN_VULKAN) << "Required Vulkan extension" << extension << "is not supported.";
            return false;
        }

        enabledInstanceExtensions.push_back(extension);
    }

    bool haveGetPhysicalDeviceProperties2 = supportedInstanceExtensions.contains(VK_KHR_GET_PHYSICAL_DEVICE_PROPERTIES_2_EXTENSION_NAME);
    if (haveGetPhysicalDeviceProperties2)
        enabledInstanceExtensions.push_back(VK_KHR_GET_PHYSICAL_DEVICE_PROPERTIES_2_EXTENSION_NAME);

    bool haveExtDebugReport = supportedInstanceExtensions.contains(VK_EXT_DEBUG_REPORT_EXTENSION_NAME);
    if (haveExtDebugReport)
        enabledInstanceExtensions.push_back(VK_EXT_DEBUG_REPORT_EXTENSION_NAME);


    // Create the vulkan instance
    // --------------------------
    const VkApplicationInfo applicationInfo {
        .sType = VK_STRUCTURE_TYPE_APPLICATION_INFO,
        .pNext = nullptr,
        .pApplicationName = KWIN_NAME,
        .applicationVersion = VK_MAKE_VERSION(KWIN_VERSION_MAJOR, KWIN_VERSION_MINOR, KWIN_VERSION_PATCH),
        .pEngineName = nullptr,
        .engineVersion = VK_MAKE_VERSION(0, 0, 0),
        .apiVersion = VK_API_VERSION_1_0
    };

    m_instance = VulkanInstance(applicationInfo, enabledLayers, enabledInstanceExtensions);
    if (!m_instance.isValid()) {
        qCCritical(KWIN_VULKAN) << "Failed to create a Vulkan instance";
        return false;
    }

    m_backend->setInstance(&m_instance);


    // Resolve all entry points
    // ------------------------
    resolveFunctions(m_instance);


    // Create the debug callback
    // -------------------------
    if (haveExtDebugReport) {
        auto debugFunc = [](VkFlags msgFlags, VkDebugReportObjectTypeEXT objType,
                            uint64_t srcObject, size_t location, int32_t msgCode,
                            const char *pLayerPrefix, const char *pMsg, void *pUserData) -> VkBool32 {
            Q_UNUSED(objType)
            Q_UNUSED(srcObject)
            Q_UNUSED(location)
            Q_UNUSED(pUserData)

            if (msgFlags & VK_DEBUG_REPORT_ERROR_BIT_EXT) {
                qCWarning(KWIN_VULKAN, "ERROR: [%s] Code %d : %s", pLayerPrefix, msgCode, pMsg);
            } else if (msgFlags & VK_DEBUG_REPORT_WARNING_BIT_EXT) {
                qCWarning(KWIN_VULKAN, "WARNING: [%s] Code %d : %s", pLayerPrefix, msgCode, pMsg);
            }

            return false;
        };

        m_debugReportCallback = VulkanDebugReportCallback(m_instance,
                                                          VK_DEBUG_REPORT_ERROR_BIT_EXT | VK_DEBUG_REPORT_WARNING_BIT_EXT,
                                                          debugFunc);
    }


    // Create the Vulkan surface
    // -------------------------
    if (!m_backend->createSurfaces()) {
        qCCritical(KWIN_VULKAN) << "Failed to create the Vulkan platform surface(s)";
        return false;
    }

    std::shared_ptr<VulkanSurface> surface = m_backend->surface();


    // Enumerate the available GPU's
    // -----------------------------
    const char *requiredDeviceExtensions[] = {
        VK_KHR_SWAPCHAIN_EXTENSION_NAME,
    };

    const char *optionalDeviceExtensions[] = {
        VK_KHR_MAINTENANCE1_EXTENSION_NAME,
        VK_KHR_EXTERNAL_MEMORY_CAPABILITIES_EXTENSION_NAME,
        VK_KHR_EXTERNAL_MEMORY_EXTENSION_NAME,
        VK_KHR_EXTERNAL_MEMORY_FD_EXTENSION_NAME,
        VK_KHR_PUSH_DESCRIPTOR_EXTENSION_NAME,
        VK_KHR_INCREMENTAL_PRESENT_EXTENSION_NAME,
        VK_KHR_DESCRIPTOR_UPDATE_TEMPLATE_EXTENSION_NAME,
        VK_KHR_MAINTENANCE2_EXTENSION_NAME,
        VK_KHR_DEDICATED_ALLOCATION_EXTENSION_NAME,
        VK_KHR_GET_MEMORY_REQUIREMENTS_2_EXTENSION_NAME,
        VK_KHR_BIND_MEMORY_2_EXTENSION_NAME,
        VK_EXT_DISCARD_RECTANGLES_EXTENSION_NAME,
        VK_EXT_EXTERNAL_MEMORY_DMA_BUF_EXTENSION_NAME,
    };

    VulkanExtensionVector supportedDeviceExtensions;

    VulkanPhysicalDevice physicalDevice;
    uint32_t graphicsQueueFamilyIndex = 0;
    uint32_t presentQueueFamilyIndex = 0;
    uint32_t bestScore = 0;

    std::vector<VulkanPhysicalDevice> devices = m_instance.enumeratePhysicalDevices();
    for (uint32_t index = 0; index < devices.size(); index++) {
        VulkanPhysicalDevice &device = devices[index];

        // Enumerate the device extensions
        supportedDeviceExtensions = device.enumerateDeviceExtensionProperties();

        // The device must support all required extensions
        if (!supportedDeviceExtensions.containsAll(requiredDeviceExtensions))
            continue;

        // The device must support sampling linear VK_FORMAT_B8G8R8A8_UNORM images.
        // This is used to upload decoration textures.
        // AMD, Intel, and NVIDIA all support this.
        VkFormatProperties formatProperties;
        device.getFormatProperties(VK_FORMAT_B8G8R8A8_UNORM, &formatProperties);

        if (!(formatProperties.linearTilingFeatures & VK_FORMAT_FEATURE_SAMPLED_IMAGE_BIT))
            continue;

        // Swapchain images must be renderable
        VkSurfaceCapabilitiesKHR capabilities;
        device.getSurfaceCapabilitiesKHR(surface->handle(), &capabilities);

        if (!(capabilities.supportedUsageFlags & VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT))
            continue;

        // Enumerate the available queue families
        const std::vector<VkQueueFamilyProperties> queueFamilies = device.getQueueFamilyProperties();

        // Find queue families that support graphics and presentation
        uint32_t graphicsIndex = UINT32_MAX;
        uint32_t presentIndex = UINT32_MAX;

        for (unsigned i = 0; i < queueFamilies.size(); i++) {
            const VkQueueFamilyProperties &family = queueFamilies[i];

            if (family.queueCount < 1)
                continue;

            // Note that the VK_QUEUE_TRANSFER_BIT does not need to be set when
            // VK_QUEUE_GRAPHICS_BIT is set; transfer support is implied
            if (graphicsIndex == UINT32_MAX && (family.queueFlags & VK_QUEUE_GRAPHICS_BIT))
                graphicsIndex = i;

            // Check if the queue family can present images to the surface
            if (presentIndex == UINT32_MAX) {
                VkBool32 supported = VK_FALSE;
                if (device.getSurfaceSupportKHR(i, surface->handle(), &supported) == VK_SUCCESS) {
                    // Check for platform specific present support
                    if (supported && m_backend->getPhysicalDevicePresentationSupport(device, i))
                        presentIndex = i;
                }
            }
        }

        // The device must support graphics and presentation
        if (graphicsIndex == UINT32_MAX || presentIndex == UINT32_MAX)
            continue;

        VkPhysicalDeviceProperties properties;
        device.getProperties(&properties);

        // Assign a score to the device based on the type
        const struct {
            VkPhysicalDeviceType type;
            int score;
        } scores[] = {
            { VK_PHYSICAL_DEVICE_TYPE_INTEGRATED_GPU, 500 },
            { VK_PHYSICAL_DEVICE_TYPE_DISCRETE_GPU,   400 },
            { VK_PHYSICAL_DEVICE_TYPE_VIRTUAL_GPU,    300 },
            { VK_PHYSICAL_DEVICE_TYPE_CPU,            200 }
        };

        uint32_t score = 100;

        const auto it = std::find_if(std::cbegin(scores), std::cend(scores), [&](const auto &entry) { return entry.type == properties.deviceType; });
        if (it != std::cend(scores))
            score = it->score;

        // Assign the highest score to the device selected by the user
        if (options->vulkanDevice() == VulkanDeviceId(index, properties.vendorID, properties.deviceID))
            score = 1000;

        if (score > bestScore) {
            bestScore                = score;
            physicalDevice           = device;
            graphicsQueueFamilyIndex = graphicsIndex;
            presentQueueFamilyIndex  = presentIndex;
        }
    }

    if (!physicalDevice.isValid()) {
        qCCritical(KWIN_VULKAN) << "Failed to find a usable GPU";
        return false;
    }


    // Get the physical device properties
    // ----------------------------------
    if (haveGetPhysicalDeviceProperties2) {
        VkPhysicalDevicePushDescriptorPropertiesKHR pushDescriptorProperties = {
            .sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_PUSH_DESCRIPTOR_PROPERTIES_KHR,
            .pNext = nullptr,
            .maxPushDescriptors = 0
        };

        VkPhysicalDeviceDiscardRectanglePropertiesEXT discardRectangleProperties = {
            .sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_DISCARD_RECTANGLE_PROPERTIES_EXT,
            .pNext = &pushDescriptorProperties,
            .maxDiscardRectangles = 0
        };

        VkPhysicalDeviceProperties2KHR physicalDeviceProperties2 = {
            .sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_PROPERTIES_2_KHR,
            .pNext = &discardRectangleProperties,
            .properties = {}
        };

        physicalDevice.getProperties2KHR(&physicalDeviceProperties2);

        m_physicalDeviceProperties = physicalDeviceProperties2.properties;
        m_maxDiscardRects = discardRectangleProperties.maxDiscardRectangles;
        m_maxPushDescriptors = pushDescriptorProperties.maxPushDescriptors;
    } else {
        physicalDevice.getProperties(&m_physicalDeviceProperties);

        m_maxDiscardRects = 0;
        m_maxPushDescriptors = 0;
    }

    qCInfo(KWIN_VULKAN).nospace() << "Vulkan API version: "
        << VK_VERSION_MAJOR(m_physicalDeviceProperties.apiVersion) << '.'
        << VK_VERSION_MINOR(m_physicalDeviceProperties.apiVersion) << '.'
        << VK_VERSION_PATCH(m_physicalDeviceProperties.apiVersion);

    qCInfo(KWIN_VULKAN).nospace().noquote() << "Driver version: "
        << driverVersionString(m_physicalDeviceProperties.vendorID, m_physicalDeviceProperties.driverVersion);

    qCInfo(KWIN_VULKAN).nospace().noquote() << "Vendor ID: " << vendorName(m_physicalDeviceProperties.vendorID);
    qCInfo(KWIN_VULKAN).nospace().noquote() << "Device ID: " << showbase << hex << m_physicalDeviceProperties.deviceID;
    qCInfo(KWIN_VULKAN).nospace().noquote() << "Device name: " << m_physicalDeviceProperties.deviceName;
    qCInfo(KWIN_VULKAN).nospace().noquote() << "Device type: " << enumToString(m_physicalDeviceProperties.deviceType);


    // Get the supported surface formats
    // ---------------------------------
    const std::vector<VkSurfaceFormatKHR> supportedFormats = physicalDevice.getSurfaceFormatsKHR(surface->handle());

    const VkFormat preferredFormats[] {
        VK_FORMAT_A2R10G10B10_UNORM_PACK32,
        VK_FORMAT_A2B10G10R10_UNORM_PACK32,
        VK_FORMAT_B8G8R8A8_UNORM,
        VK_FORMAT_R8G8B8A8_UNORM,
        VK_FORMAT_A8B8G8R8_UNORM_PACK32,
        VK_FORMAT_B8G8R8A8_SRGB,
        VK_FORMAT_R8G8B8A8_SRGB,
        VK_FORMAT_A8B8G8R8_SRGB_PACK32,
        VK_FORMAT_B8G8R8_UNORM,
        VK_FORMAT_R8G8B8_UNORM,
        VK_FORMAT_B8G8R8_SRGB,
        VK_FORMAT_R8G8B8_SRGB,
    };

    m_surfaceFormat = VK_FORMAT_UNDEFINED;
    m_surfaceColorSpace = VK_COLORSPACE_SRGB_NONLINEAR_KHR;

    if (supportedFormats.size() == 1 && supportedFormats[0].format == VK_FORMAT_UNDEFINED) {
        // The surface supports any format - pick the first in our list of preferred formats
        m_surfaceFormat = preferredFormats[0];

        qCDebug(KWIN_VULKAN).noquote() << "Using" << enumToString(m_surfaceFormat) << "for swapchain images";
    } else if (supportedFormats.size() > 0) {
        // Default to the first supported format in case none of our preferred formats are supported
        m_surfaceFormat = supportedFormats[0].format;
        m_surfaceColorSpace = supportedFormats[0].colorSpace;

        // Find the first supported format in preferredFormats
        for (VkFormat preferred : preferredFormats) {
            auto it = std::find_if(supportedFormats.begin(), supportedFormats.end(),
                                   [=](const VkSurfaceFormatKHR &supported) { return supported.format == preferred; });
            if (it != supportedFormats.end()) {
                m_surfaceFormat = it->format;
                m_surfaceColorSpace = it->colorSpace;
                break;
            }
        }

        qCInfo(KWIN_VULKAN) << "Supported surface formats:";
        for (const VkSurfaceFormatKHR &supported : supportedFormats) {
            if (supported.format == m_surfaceFormat)
                qCInfo(KWIN_VULKAN).nospace().noquote() << "  > " << enumToString(supported.format);
            else
                qCInfo(KWIN_VULKAN).nospace().noquote() << "    " << enumToString(supported.format);
        }
    } else {
        qCCritical(KWIN_VULKAN) << "The surface does not support any image formats";
        return false;
    }


    // Get the supported present modes
    // -------------------------------
    const std::vector<VkPresentModeKHR> supportedPresentModes = physicalDevice.getSurfacePresentModesKHR(surface->handle());
    uint32_t swapchainImageCount;

    if (supportedPresentModes.size() > 0) {
        const std::array<VkPresentModeKHR, 4> presentModes[] = {
            // VSync off
            {
                VK_PRESENT_MODE_IMMEDIATE_KHR,
                VK_PRESENT_MODE_FIFO_RELAXED_KHR,
                VK_PRESENT_MODE_FIFO_KHR,
                VK_PRESENT_MODE_MAILBOX_KHR,
            },
            // Double buffer
            {
                VK_PRESENT_MODE_FIFO_KHR,
                VK_PRESENT_MODE_MAILBOX_KHR,
                VK_PRESENT_MODE_FIFO_RELAXED_KHR,
                VK_PRESENT_MODE_IMMEDIATE_KHR
            },
            // Triple buffer
            {
                VK_PRESENT_MODE_MAILBOX_KHR,
                VK_PRESENT_MODE_FIFO_KHR,
                VK_PRESENT_MODE_FIFO_RELAXED_KHR,
                VK_PRESENT_MODE_IMMEDIATE_KHR
            },
        };

        const auto &preferredPresentModes = presentModes[options->vulkanVsync()];

        // Find the first supported mode in preferredPresentModes
        auto result = std::find_first_of(preferredPresentModes.begin(), preferredPresentModes.end(),
                                         supportedPresentModes.begin(), supportedPresentModes.end());

        if (result != preferredPresentModes.end())
            m_presentMode = *result;
        else
            m_presentMode = supportedPresentModes[0];

        switch (m_presentMode) {
        case VK_PRESENT_MODE_IMMEDIATE_KHR:
        case VK_PRESENT_MODE_FIFO_KHR:
        case VK_PRESENT_MODE_FIFO_RELAXED_KHR:
        default:
            swapchainImageCount = 2;
            break;

        case VK_PRESENT_MODE_MAILBOX_KHR:
            swapchainImageCount = 3;
            break;
        }

        qCInfo(KWIN_VULKAN) << "Supported present modes:";
        for (const VkPresentModeKHR mode : supportedPresentModes) {
            if (mode == m_presentMode)
                qCInfo(KWIN_VULKAN).nospace().noquote() << "  > " << enumToString(mode);
            else
                qCInfo(KWIN_VULKAN).nospace().noquote() << "    " << enumToString(mode);
        }
    } else {
        qCCritical(KWIN_VULKAN) << "The physical device does not support any presentation modes.";
        return false;
    }


    // Create the logical device
    // -------------------------
    const float queuePriorities[] { 1.0f };
    std::vector<const char *> enabledDeviceExtensions;

    for (const char *name : requiredDeviceExtensions)
        enabledDeviceExtensions.push_back(name);

    for (const char *name : optionalDeviceExtensions) {
        if (supportedDeviceExtensions.contains(name)) {
            enabledDeviceExtensions.push_back(name);
        }
    }

    m_separatePresentQueue = graphicsQueueFamilyIndex != presentQueueFamilyIndex;

    const VkDeviceQueueCreateInfo queueCreateInfo[] = {
        {
            .sType = VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO,
            .pNext = nullptr,
            .flags = 0,
            .queueFamilyIndex = graphicsQueueFamilyIndex,
            .queueCount = 1,
            .pQueuePriorities = queuePriorities
        },
        {
            .sType = VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO,
            .pNext = nullptr,
            .flags = 0,
            .queueFamilyIndex = presentQueueFamilyIndex,
            .queueCount = 1,
            .pQueuePriorities = queuePriorities
        }
    };

    const VkPhysicalDeviceFeatures enabledFeatures = {};

    const VkDeviceCreateInfo deviceCreateInfo = {
        .sType = VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO,
        .pNext = nullptr,
        .flags = 0,
        .queueCreateInfoCount = m_separatePresentQueue ? 2u : 1u,
        .pQueueCreateInfos = queueCreateInfo,
        .enabledLayerCount = 0,
        .ppEnabledLayerNames = nullptr,
        .enabledExtensionCount = (uint32_t) enabledDeviceExtensions.size(),
        .ppEnabledExtensionNames = enabledDeviceExtensions.data(),
        .pEnabledFeatures = &enabledFeatures
    };

    m_device = std::make_unique<VulkanDevice>(physicalDevice, &deviceCreateInfo);
    if (!m_device->isValid()) {
        qCCritical(KWIN_VULKAN) << "Failed to create the logical device";
        return false;
    }

    // Get the queues from the device
    m_graphicsQueue = m_device->getQueue(graphicsQueueFamilyIndex, 0);

    if (m_separatePresentQueue)
        m_presentQueue = m_device->getQueue(presentQueueFamilyIndex, 0);
    else
        m_presentQueue = m_graphicsQueue;


    // Report the enabled layers & extensions
    qCInfo(KWIN_VULKAN) << "Enabled instance layers:";
    for (auto &layer : enabledLayers)
        qCInfo(KWIN_VULKAN) << "    " << layer;

    qCInfo(KWIN_VULKAN) << "Enabled instance extensions:";
    for (auto &extension : enabledInstanceExtensions)
        qCInfo(KWIN_VULKAN) << "    " << extension;

    qCInfo(KWIN_VULKAN) << "Enabled device extensions:";
    for (auto &extension : enabledDeviceExtensions)
        qCInfo(KWIN_VULKAN) << "    " << extension;

    m_haveMaintenance1 = supportedDeviceExtensions.contains(VK_KHR_MAINTENANCE1_EXTENSION_NAME);

    // Create the command pools
    // ------------------------
    if (m_separatePresentQueue)
        m_presentCommandPool = VulkanCommandPool(device(), 0, m_presentQueue.familyIndex());


    // Create the render passes
    // ------------------------
    if (!createRenderPasses()) {
        qCCritical(KWIN_VULKAN) << "Failed to create the render passes";
        return false;
    }


    // Create the samplers
    // -------------------
    m_nearestSampler =
            VulkanSampler(device(),
                          {
                              .sType = VK_STRUCTURE_TYPE_SAMPLER_CREATE_INFO,
                              .pNext = nullptr,
                              .flags = 0,
                              .magFilter = VK_FILTER_NEAREST,
                              .minFilter = VK_FILTER_NEAREST,
                              .mipmapMode = VK_SAMPLER_MIPMAP_MODE_NEAREST,
                              .addressModeU = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE,
                              .addressModeV = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE,
                              .addressModeW = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE,
                              .mipLodBias = 0.0f,
                              .anisotropyEnable = VK_FALSE,
                              .maxAnisotropy = 1.0f,
                              .compareEnable = VK_FALSE,
                              .compareOp = VK_COMPARE_OP_ALWAYS,
                              .minLod = 0.0f,
                              .maxLod = 1.0f,
                              .borderColor = VK_BORDER_COLOR_FLOAT_TRANSPARENT_BLACK,
                              .unnormalizedCoordinates = VK_TRUE
                          });

    m_linearSampler =
            VulkanSampler(device(),
                          {
                              .sType = VK_STRUCTURE_TYPE_SAMPLER_CREATE_INFO,
                              .pNext = nullptr,
                              .flags = 0,
                              .magFilter = VK_FILTER_LINEAR,
                              .minFilter = VK_FILTER_LINEAR,
                              .mipmapMode = VK_SAMPLER_MIPMAP_MODE_NEAREST,
                              .addressModeU = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE,
                              .addressModeV = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE,
                              .addressModeW = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE,
                              .mipLodBias = 0.0f,
                              .anisotropyEnable = VK_FALSE,
                              .maxAnisotropy = 1.0f,
                              .compareEnable = VK_FALSE,
                              .compareOp  = VK_COMPARE_OP_ALWAYS,
                              .minLod = 0.0f,
                              .maxLod = 1.0f,
                              .borderColor = VK_BORDER_COLOR_FLOAT_TRANSPARENT_BLACK,
                              .unnormalizedCoordinates = VK_TRUE
                          });


    // Create the swapchain
    // --------------------
    m_swapchain = std::make_unique<VulkanSwapchain>(device(), m_presentQueue, surface->handle(), m_surfaceFormat, m_surfaceColorSpace, m_presentMode, m_loadRenderPass);
    m_swapchain->setSupportsIncrementalPresent(supportedDeviceExtensions.contains(VK_KHR_INCREMENTAL_PRESENT_EXTENSION_NAME));
    m_swapchain->setPreferredImageCount(swapchainImageCount);


    // Create the pipeline manager
    // ---------------------------
    m_pipelineCache = std::make_unique<VulkanPipelineCache>(device());

    m_pipelineManager = std::make_unique<VulkanPipelineManager>(device(), m_pipelineCache.get(),
                                                                m_nearestSampler, m_linearSampler,
                                                                m_loadRenderPass, (VkRenderPass) VK_NULL_HANDLE,
                                                                supportedDeviceExtensions.contains(VK_KHR_PUSH_DESCRIPTOR_EXTENSION_NAME));

    if (!m_pipelineManager->isValid()) {
        qCCritical(KWIN_VULKAN) << "Failed to create the Vulkan pipeline manager";
        return false;
    }


    // Prepare the frame & paint pass ring buffers
    // -------------------------------------------
    for (FrameData &data : m_frameData) {
        data.imageAcquisitionFence = VulkanFence(device());
        data.imageAcquisitionFenceSubmitted = false;

        data.imageAcquisitionSemaphore = std::make_shared<VulkanSemaphore>(device());

        if (m_separatePresentQueue) {
            data.acquireOwnershipSemaphore = std::make_shared<VulkanSemaphore>(device());
            data.releaseOwnershipSemaphore = std::make_shared<VulkanSemaphore>(device());
        }
    }

    for (PaintPassData &data : m_paintPassData) {
        data.semaphore = std::make_shared<VulkanSemaphore>(device());
        data.commandPools[GraphicsQueueFamily] = VulkanCommandPool(device(), VK_COMMAND_POOL_CREATE_TRANSIENT_BIT, m_graphicsQueue.familyIndex());
        data.setupCommandBuffer = std::make_unique<VulkanCommandBuffer>(device(), data.commandPools[GraphicsQueueFamily], VK_COMMAND_BUFFER_LEVEL_PRIMARY);
        data.mainCommandBuffer = std::make_unique<VulkanCommandBuffer>(device(), data.commandPools[GraphicsQueueFamily], VK_COMMAND_BUFFER_LEVEL_PRIMARY);
        data.fence = VulkanFence(device());
        data.fenceSubmitted = false;
    }


    // Create the device memory allocator
    // ----------------------------------
    m_deviceMemoryAllocator = std::make_unique<VulkanDeviceMemoryAllocator>(device(), enabledDeviceExtensions);


    // Create the upload managers
    // --------------------------
    m_uploadManager =
            std::make_unique<VulkanUploadManager>(device(),
                                                  deviceMemoryAllocator(),
                                                  16 * 1024,
                                                  VK_BUFFER_USAGE_TRANSFER_SRC_BIT | VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT | VK_BUFFER_USAGE_VERTEX_BUFFER_BIT,
                                                  VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT,
                                                  m_physicalDeviceProperties.limits);

    m_imageUploadManager =
            std::make_unique<VulkanUploadManager>(device(),
                                                  deviceMemoryAllocator(),
                                                  8 * 1024 * 1024,
                                                  VK_BUFFER_USAGE_TRANSFER_SRC_BIT,
                                                  VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT,
                                                  m_physicalDeviceProperties.limits);


    // Create the descriptor pools
    // ---------------------------
    m_textureDescriptorPool =
            std::make_unique<SceneDescriptorPool>(SceneDescriptorPool::CreateInfo {
                                                      .device = device(),
                                                      .descriptorSetLayout =
                                                              pipelineManager()->descriptorSetLayout(VulkanPipelineManager::Texture),
                                                      .maxSets = 72,
                                                      .poolSizes = {
                                                          { VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, 72 },
                                                          { VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER_DYNAMIC, 72 }
                                                      }
                                                  });

    m_crossFadeDescriptorPool =
            std::make_unique<SceneDescriptorPool>(SceneDescriptorPool::CreateInfo {
                                                     .device = device(),
                                                     .descriptorSetLayout =
                                                             pipelineManager()->descriptorSetLayout(VulkanPipelineManager::TwoTextures),
                                                     .maxSets = 8,
                                                     .poolSizes = {
                                                         { VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE,          16 },
                                                         { VK_DESCRIPTOR_TYPE_SAMPLER,                 8 },
                                                         { VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER_DYNAMIC,  8 }
                                                     }
                                                  });

    m_colorDescriptorPool =
            std::make_unique<SceneDescriptorPool>(SceneDescriptorPool::CreateInfo {
                                                      .device = device(),
                                                      .descriptorSetLayout =
                                                              pipelineManager()->descriptorSetLayout(VulkanPipelineManager::FlatColor),
                                                      .maxSets = 16,
                                                      .poolSizes = {
                                                          { VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER_DYNAMIC, 16 }
                                                      }
                                                  });

    m_valid = true;
    return m_valid;
}


bool VulkanScene::usesOverlayWindow() const
{
    return m_backend->usesOverlayWindow();
}


OverlayWindow *VulkanScene::overlayWindow()
{
    return m_backend->overlayWindow();
}


// These functions create projection matrices compatible with the Vulkan coordinate system
//
// In Vulkan the normalized device coordinates (-1, -1) correspond to the uppper-left corner
// of the viewport, and the near and far clip planes correspond to 0 and +1 respectively.
// QMatrix4x4::ortho() and QMatrix4x4::frustum() create matrices for the OpenGL coordinate
// system, where (-1, -1) correspond to the lower-left corner, and the near and far clip
// planes correspond to -1 and +1. respectively
QMatrix4x4 VulkanScene::ortho(float left, float right, float bottom, float top, float near, float far) const
{
    return QMatrix4x4(2.0f / (right - left), 0.0f,                  0.0f,                -(right + left) / (right - left),
                      0.0f,                  2.0f / (bottom - top), 0.0f,                -(top + bottom) / (bottom - top),
                      0.0f,                  0.0f,                  1.0f / (near - far),  near / (near - far),
                      0.0f,                  0.0f,                  0.0f,                 1.0f);
}


QMatrix4x4 VulkanScene::frustum(float left, float right, float bottom, float top, float zNear, float zFar) const
{
    const float x = 2.0f * zNear   / (right  - left);
    const float y = 2.0f * zNear   / (bottom - top);
    const float a = (right + left) / (right  - left);
    const float b = (top + bottom) / (bottom - top);
    const float c = zFar           / (zNear  - zFar);
    const float d = zNear * zFar   / (zNear  - zFar);

    return QMatrix4x4(x,     0.0f,  a,     0.0f,
                      0.0f,  y,     b,     0.0f,
                      0.0f,  0.0f,  c,     d,
                      0.0f,  0.0f, -1.0f,  0.0f);
}


QMatrix4x4 VulkanScene::createProjectionMatrix() const
{
    // Create a perspective projection with a 60° field-of-view,
    // and an aspect ratio of 1.0.
    const float fovY   =   60.0f;
    const float aspect =    1.0f;
    const float zNear  =    0.1f;
    const float zFar   =  100.0f;

    const float yMax   =  zNear * std::tan(fovY * M_PI / 360.0f);
    const float yMin   = -yMax;
    const float xMin   =  yMin * aspect;
    const float xMax   =  yMax * aspect;

    const QMatrix4x4 projection = frustum(xMin, xMax, yMin, yMax, zNear, zFar);

    // Create a second matrix that transforms screen coordinates
    // to world coordinates.
    const float scaleFactor = 1.1 * std::tan(fovY * M_PI / 360.0f) / yMax;
    const QSize size = screens()->size();

    QMatrix4x4 matrix;
    matrix.translate(xMin * scaleFactor, yMax * scaleFactor, -1.1);
    matrix.scale( (xMax - xMin) * scaleFactor / size.width(),
                 -(yMax - yMin) * scaleFactor / size.height(),
                  0.001);

    // Combine the matrices
    return projection * matrix;
}


QMatrix4x4 VulkanScene::screenMatrix(int mask, const ScreenPaintData &data) const
{
    QMatrix4x4 matrix;

    if (!(mask & PAINT_SCREEN_TRANSFORMED))
        return matrix;

    matrix.translate(data.translation());
    data.scale().applyTo(&matrix);

    if (data.rotationAngle() == 0.0)
        return matrix;

    // Apply the rotation
    //
    // Note that we cannot use data.rotation->applyTo(&matrix) as QGraphicsRotation
    // uses projectedRotate() to map back to 2D
    matrix.translate(data.rotationOrigin());
    const QVector3D axis = data.rotationAxis();
    matrix.rotate(data.rotationAngle(), axis.x(), axis.y(), axis.z());
    matrix.translate(-data.rotationOrigin());

    return matrix;
}


void VulkanScene::handleDeviceLostError()
{
    qCDebug(KWIN_VULKAN) << "Attempting to reset compositing following VK_ERROR_DEVICE_LOST.";
    QMetaObject::invokeMethod(this, "resetCompositing", Qt::QueuedConnection);

    KNotification::event(QStringLiteral("graphicsreset"), i18n("Desktop effects were restarted due to a graphics reset"));
}


void VulkanScene::handleFatalError()
{
    QMetaObject::invokeMethod(this, "compositingFailed", Qt::QueuedConnection);
    KNotification::event(QStringLiteral("compositingfailed"),
                         i18n("Desktop effects have been suspended due to a fatal error"));
}


bool VulkanScene::checkResult(VkResult result, std::function<void(void)> printDebugMessage)
{
    switch (result) {
    case VK_SUCCESS:
        return true;

    case VK_ERROR_DEVICE_LOST:
        handleDeviceLostError();
        return false;

    case VK_ERROR_OUT_OF_HOST_MEMORY:
    case VK_ERROR_OUT_OF_DEVICE_MEMORY:
    case VK_ERROR_SURFACE_LOST_KHR:
    default:
        printDebugMessage();
        handleFatalError();
        return false;
    }
}


struct ScopeGuard
{
    ~ScopeGuard() { destructor(); }
    std::function<void(void)> destructor;
};


qint64 VulkanScene::paint(QRegion damage, ToplevelList windows)
{
    QElapsedTimer timer;
    timer.start();

    createStackingOrder(windows);

    ScopeGuard stackingOrderGuard {[this]{ clearStackingOrder(); }};

    if (m_backend->perScreenRendering()) {
        // TODO
    } else {
        int mask = 0;

        // (Re)create the swap chain if necessary
        if (!m_swapchain->isValid()) {
            const VkResult result = m_swapchain->create();

            if (!checkResult(result, []{ qCCritical(KWIN_VULKAN) << "Swapchain creation failed"; }))
                return 0;

            m_imageAcquired = false;
        }

        FrameData &frame = m_frameData[m_frameIndex];

        if (!m_imageAcquired) {
            if (frame.imageAcquisitionFenceSubmitted) {
                // Block on the acquisition fence to prevent us from getting more than
                // m_frameData.size() frames ahead of the presentation engine.
                frame.imageAcquisitionFence.wait();
                frame.imageAcquisitionFence.reset();

                frame.imageAcquisitionFenceSubmitted = false;
            }

            // Attempt to acquire an image from the swapchain
            int32_t imageIndex = -1;
            VkResult result = m_swapchain->acquireNextImage(UINT64_MAX, frame.imageAcquisitionSemaphore->handle(), frame.imageAcquisitionFence, &imageIndex);

            switch (result) {
            case VK_SUCCESS:
                break;

            case VK_SUBOPTIMAL_KHR:
                // Schedule a full repaint and invalidate the swapchain
                // so it is recreated in the next paint pass.
                m_swapchain->invalidate();
                effects->addRepaintFull();
                break;

            case VK_ERROR_OUT_OF_DATE_KHR:
                effects->addRepaintFull();
                return 0;

            case VK_ERROR_DEVICE_LOST:
                handleDeviceLostError();
                return 0;

            default:
                qCCritical(KWIN_VULKAN).noquote() << "vkAcquireNextImageKHR returned" << enumToString(result);
                handleFatalError();
                return 0;
            };

            m_waitSemaphores.insert(frame.imageAcquisitionSemaphore);
            frame.imageAcquisitionFenceSubmitted = true;

            m_bufferAge = m_swapchain->bufferAge();
            m_surfaceLayout = m_bufferAge == 0 ? VK_IMAGE_LAYOUT_UNDEFINED : VK_IMAGE_LAYOUT_PRESENT_SRC_KHR;
            m_needQueueOwnershipTransfer = false; // Will depend on whether we load or clear the contents
            m_imageAcquired = true;
        }

        QRegion repaint = m_swapchain->accumulatedDamageHistory(m_bufferAge);

        m_projectionMatrix = createProjectionMatrix();
        m_renderPassStarted = false;
        m_clearPending = false;
        m_usedIndexBuffer = false;

        PaintPassData &paintPass = m_paintPassData[m_paintPassIndex];

        if (paintPass.fenceSubmitted) {
            paintPass.fence.wait();

            // Reset the command pools
            for (auto &pool : paintPass.commandPools)
                pool.reset();

            // Clear lists of Vulkan objects referenced by this paint pass
            paintPass.busyObjects.clear();

            paintPass.fence.reset();
            paintPass.fenceSubmitted = false;
        }


        // After this call, updateRegion will contain the damaged region in the
        // back buffer. This is the region that needs to be posted to repair
        // the front buffer. It does not include the additional damage returned
        // by accumulatedDamageHistory(). validRegion is the region that has been
        // repainted, and it may be larger than updateRegion.
        QRegion updateRegion, validRegion;
        paintScreen(&mask, damage, repaint, &updateRegion, &validRegion, projectionMatrix());

        updateRegion &= QRegion(0, 0, m_swapchain->width(), m_swapchain->height());


        // Submit the command buffers
        // --------------------------
        if (m_commandBuffersPending || m_clearPending) {
            VulkanCommandBuffer *setupCommandBuffer = paintPass.setupCommandBuffer.get();
            VulkanCommandBuffer *mainCommandBuffer = paintPass.mainCommandBuffer.get();

            std::vector<VkSemaphore> waitSemaphores = m_waitSemaphores.toNativeHandleVector();

            // If the present queue has ownership of the image we need to submit a
            // command buffer to the present queue that relinquishes ownership before
            // we submit the main command buffer to the graphics queue. The graphics
            // queue ownership acquisition operation is handled by beginRenderPass().
            if (m_needQueueOwnershipTransfer) {
                std::shared_ptr<VulkanCommandBuffer> &cmd = m_swapchain->releaseOwnershipCommandBuffer();

                // This command buffer is recorded once and reused
                if (!cmd) {
                    cmd = std::make_shared<VulkanCommandBuffer>(device(), m_presentCommandPool, VK_COMMAND_BUFFER_LEVEL_PRIMARY);

                    cmd->begin(VK_COMMAND_BUFFER_USAGE_SIMULTANEOUS_USE_BIT);
                    cmd->pipelineBarrier(VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT,
                                         VK_PIPELINE_STAGE_BOTTOM_OF_PIPE_BIT,
                                         0,
                                         {},
                                         {},
                                         {
                                             {
                                                 .sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER,
                                                 .pNext = nullptr,
                                                 .srcAccessMask = VK_ACCESS_MEMORY_READ_BIT,
                                                 .dstAccessMask = 0,
                                                 .oldLayout = VK_IMAGE_LAYOUT_PRESENT_SRC_KHR,
                                                 .newLayout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL,
                                                 .srcQueueFamilyIndex = m_presentQueue.familyIndex(),
                                                 .dstQueueFamilyIndex = m_graphicsQueue.familyIndex(),
                                                 .image = m_swapchain->currentImage(),
                                                 .subresourceRange = {
                                                     .aspectMask = VK_IMAGE_ASPECT_COLOR_BIT,
                                                     .baseMipLevel = 0,
                                                     .levelCount = 1,
                                                     .baseArrayLayer = 0,
                                                     .layerCount = 1
                                                 }
                                             }
                                         });
                    cmd->end();
                }

                // Submit the command buffer to the present queue
                const VkCommandBuffer commandBuffers[] = { cmd->handle() };
                const VkSemaphore signalSemaphores[] = { frame.releaseOwnershipSemaphore->handle() };
                std::vector<VkPipelineStageFlags> waitDstStageMasks(waitSemaphores.size(), VK_PIPELINE_STAGE_ALL_COMMANDS_BIT);

                const VkResult result = m_presentQueue.submit({
                                                                  {
                                                                      .sType = VK_STRUCTURE_TYPE_SUBMIT_INFO,
                                                                      .pNext = nullptr,
                                                                      .waitSemaphoreCount = (uint32_t) waitSemaphores.size(),
                                                                      .pWaitSemaphores = waitSemaphores.data(),
                                                                      .pWaitDstStageMask = waitDstStageMasks.data(),
                                                                      .commandBufferCount = 1,
                                                                      .pCommandBuffers = commandBuffers,
                                                                      .signalSemaphoreCount = 1,
                                                                      .pSignalSemaphores= signalSemaphores
                                                                  }
                                                              });

                if (!checkResult(result, [=]{ qCCritical(KWIN_VULKAN).noquote() << "VkQueueSubmit returned" << enumToString(result); }))
                    return 0;

                waitSemaphores = { signalSemaphores[0] };
                m_needQueueOwnershipTransfer = false;
            }

            // Force the clear render pass to begin
            if (m_clearPending && !m_renderPassStarted)
                beginRenderPass(mainCommandBuffer);

            if (mainCommandBuffer->isRenderPassActive())
                mainCommandBuffer->endRenderPass();

            m_renderPassStarted = false;

            // Transition the image to VK_IMAGE_LAYOUT_PRESENT_SRC_KHR
            if (!updateRegion.isEmpty() && m_surfaceLayout != VK_IMAGE_LAYOUT_PRESENT_SRC_KHR) {
                uint32_t srcAccessMask = VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT;
                uint32_t dstAccessMask = VK_ACCESS_MEMORY_READ_BIT;
                uint32_t srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
                uint32_t dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;

                if (m_separatePresentQueue) {
                    // Transfer ownership to the present queue
                    srcAccessMask = VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT;
                    dstAccessMask = 0;
                    srcQueueFamilyIndex = m_graphicsQueue.familyIndex();
                    dstQueueFamilyIndex = m_presentQueue.familyIndex();
                }

                const VkImageMemoryBarrier barrier = {
                    .sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER,
                    .pNext = nullptr,
                    .srcAccessMask = srcAccessMask,
                    .dstAccessMask = dstAccessMask,
                    .oldLayout = m_surfaceLayout,
                    .newLayout = VK_IMAGE_LAYOUT_PRESENT_SRC_KHR,
                    .srcQueueFamilyIndex = srcQueueFamilyIndex,
                    .dstQueueFamilyIndex = dstQueueFamilyIndex,
                    .image = m_swapchain->currentImage(),
                    .subresourceRange = {
                        .aspectMask = VK_IMAGE_ASPECT_COLOR_BIT,
                        .baseMipLevel = 0,
                        .levelCount = 1,
                        .baseArrayLayer = 0,
                        .layerCount = 1
                    }
                };

                assert(mainCommandBuffer->isActive());
                mainCommandBuffer->pipelineBarrier(VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT,
                                                   VK_PIPELINE_STAGE_BOTTOM_OF_PIPE_BIT,
                                                   0, {}, {}, { barrier });
                m_surfaceLayout = VK_IMAGE_LAYOUT_PRESENT_SRC_KHR;
            }


            // Flush the upload managers
            std::vector<VkMappedMemoryRange> ranges;

            m_uploadManager->getNonCoherentAllocatedRanges(std::back_inserter(ranges));
            m_imageUploadManager->getNonCoherentAllocatedRanges(std::back_inserter(ranges));

            if (!ranges.empty())
                m_device->flushMappedMemoryRanges(ranges.size(), ranges.data());

            addBusyReference(m_uploadManager->createFrameBoundary());
            addBusyReference(m_imageUploadManager->createFrameBoundary());


            // Submit the command buffers to the graphics queue
            VkCommandBuffer commandBuffers[2];
            uint32_t commandBufferCount = 0;

            if (setupCommandBuffer->isActive()) {
                commandBuffers[commandBufferCount++] = setupCommandBuffer->handle();
                setupCommandBuffer->end();
            }

            if (mainCommandBuffer->isActive()) {
                commandBuffers[commandBufferCount++] = mainCommandBuffer->handle();
                mainCommandBuffer->end();
            }

            assert(commandBufferCount > 0);

            const VkSemaphore signalSemaphores[] = { paintPass.semaphore->handle() };
            std::vector<VkPipelineStageFlags> waitDstStageMasks(waitSemaphores.size(), VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT);

            const VkResult result = m_graphicsQueue.submit({
                                                               {
                                                                   .sType = VK_STRUCTURE_TYPE_SUBMIT_INFO,
                                                                   .pNext = nullptr,
                                                                   .waitSemaphoreCount = (uint32_t) waitSemaphores.size(),
                                                                   .pWaitSemaphores = waitSemaphores.data(),
                                                                   .pWaitDstStageMask = waitDstStageMasks.data(),
                                                                   .commandBufferCount = commandBufferCount,
                                                                   .pCommandBuffers = commandBuffers,
                                                                   .signalSemaphoreCount = uint32_t(updateRegion.isEmpty() ? 0 : 1),
                                                                   .pSignalSemaphores = updateRegion.isEmpty() ? nullptr : signalSemaphores
                                                               }
                                                           },
                                                           paintPass.fence);

            m_commandBuffersPending = false;

            if (!checkResult(result, [=]{ qCCritical(KWIN_VULKAN).noquote() << "vkQueueSubmit returned" << enumToString(result); }))
                return 0;

            // Acquire present queue ownership of the swapchain image
            if (m_separatePresentQueue && !updateRegion.isEmpty()) {
                std::shared_ptr<VulkanCommandBuffer> &cmd = m_swapchain->acquireOwnershipCommandBuffer();

                // This command buffer is recorded once and reused
                if (!cmd) {
                    cmd = std::make_shared<VulkanCommandBuffer>(device(), m_presentCommandPool, VK_COMMAND_BUFFER_LEVEL_PRIMARY);

                    // Note that we never do this unless we have rendered to the image
                    // and we are about to present it, so we know that the source layout
                    // is VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL.
                    cmd->begin(VK_COMMAND_BUFFER_USAGE_SIMULTANEOUS_USE_BIT);
                    cmd->pipelineBarrier(VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT,
                                         VK_PIPELINE_STAGE_BOTTOM_OF_PIPE_BIT,
                                         0,
                                         {},
                                         {},
                                         {
                                             {
                                                 .sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER,
                                                 .pNext = nullptr,
                                                 .srcAccessMask = 0,
                                                 .dstAccessMask = 0,
                                                 .oldLayout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL,
                                                 .newLayout = VK_IMAGE_LAYOUT_PRESENT_SRC_KHR,
                                                 .srcQueueFamilyIndex = m_graphicsQueue.familyIndex(),
                                                 .dstQueueFamilyIndex = m_presentQueue.familyIndex(),
                                                 .image = m_swapchain->currentImage(),
                                                 .subresourceRange = {
                                                     .aspectMask = VK_IMAGE_ASPECT_COLOR_BIT,
                                                     .baseMipLevel = 0,
                                                     .levelCount = 1,
                                                     .baseArrayLayer = 0,
                                                     .layerCount = 1
                                                 }
                                             }
                                         });
                    cmd->end();
                }

                // Submit the command buffer to the present queue
                const VkCommandBuffer commandBuffers[] = { cmd->handle() };
                const VkSemaphore waitSemaphores[] = { paintPass.semaphore->handle() };
                const VkPipelineStageFlags waitDstStageMasks[] = { VK_PIPELINE_STAGE_ALL_COMMANDS_BIT };
                const VkSemaphore signalSemaphores[] = { frame.acquireOwnershipSemaphore->handle() };

                const VkResult result = m_presentQueue.submit({
                                                                  {
                                                                      .sType = VK_STRUCTURE_TYPE_SUBMIT_INFO,
                                                                      .pNext = nullptr,
                                                                      .waitSemaphoreCount = 1,
                                                                      .pWaitSemaphores = waitSemaphores,
                                                                      .pWaitDstStageMask = waitDstStageMasks,
                                                                      .commandBufferCount = 1,
                                                                      .pCommandBuffers = commandBuffers,
                                                                      .signalSemaphoreCount = 1,
                                                                      .pSignalSemaphores = signalSemaphores
                                                                  }
                                                              });

                if (!checkResult(result, [=]{ qCCritical(KWIN_VULKAN).noquote() << "VkQueueSubmit returned" << enumToString(result); }))
                    return 0;
            }

            paintPass.fenceSubmitted = true;

            m_waitSemaphores.clear();
            m_paintPassIndex = (m_paintPassIndex + 1) % m_paintPassData.size();
        }


        // Present the image
        // -----------------
        if (!updateRegion.isEmpty()) {
            m_backend->showOverlay();

            std::shared_ptr<VulkanSemaphore> semaphore = m_separatePresentQueue ?
                    frame.acquireOwnershipSemaphore : paintPass.semaphore;

            const VkResult result = m_swapchain->present(m_swapchain->currentIndex(), updateRegion, semaphore->handle());
            m_imageAcquired = false;

            switch (result) {
            case VK_SUCCESS:
                break;

            case VK_SUBOPTIMAL_KHR:
                m_swapchain->invalidate();
                effects->addRepaintFull();
                break;

            case VK_ERROR_OUT_OF_DATE_KHR:
                effects->addRepaintFull();
                m_waitSemaphores.insert(semaphore);
                break;

            case VK_ERROR_DEVICE_LOST:
                handleDeviceLostError();
                return 0;

            default:
            case VK_ERROR_OUT_OF_HOST_MEMORY:
            case VK_ERROR_OUT_OF_DEVICE_MEMORY:
            case VK_ERROR_SURFACE_LOST_KHR:
                qCCritical(KWIN_VULKAN).noquote() << "vkQueuePresentKHR returned" << enumToString(result);
                handleFatalError();
                return 0;
            }

            m_swapchain->addToDamageHistory(updateRegion);

            m_frameIndex = (m_frameIndex + 1) % m_frameData.size();
        } else if (paintPass.fenceSubmitted) {
            // We have executed a paint pass, but updateRegion is empty.
            // This means that any rendering done will only have repaired
            // the back buffer, making it identical to the front buffer.
            //
            // In this case we won't post the back buffer. Instead we'll just
            // set the buffer age to one, so the repaired regions won't be
            // rendered again in the next paint pass.
            m_bufferAge = 1;
        }
    }

    return timer.nsecsElapsed();
}


void VulkanScene::paintSimpleScreen(int mask, QRegion region)
{
    m_screenProjectionMatrix = m_projectionMatrix;

    Scene::paintSimpleScreen(mask, region);
}


void VulkanScene::paintGenericScreen(int mask, ScreenPaintData data)
{
    m_screenProjectionMatrix = m_projectionMatrix * screenMatrix(mask, data);

    Scene::paintGenericScreen(mask, data);
}


void VulkanScene::paintBackground(QRegion region)
{
    const QRegion swapchainRegion(0, 0, m_swapchain->width(), m_swapchain->height());
    region &= swapchainRegion;

    if (region.isEmpty())
        return;

    if (region == swapchainRegion && !m_renderPassStarted) {
        // If we haven't started the render pass yet, this is the initial clearing
        // of the back buffer. Instead of clearing the buffer here, we will mark
        // the clear as pending, and let beginRenderPass() handle it.
        m_clearPending = true;
        return;
    }

    auto vbo = uploadManager()->allocate(region.rectCount() * 4 * sizeof(QVector2D));
    auto ubo = uploadManager()->emplaceUniform<VulkanPipelineManager::ColorUniformData>(m_projectionMatrix, QVector4D(0.0f, 0.0f, 0.0f, 1.0f));

    QVector2D *vertex = static_cast<QVector2D *>(vbo.data());
    for (const QRect &r : region) {
        const float x0 = r.x();
        const float y0 = r.y();
        const float x1 = r.x() + r.width();
        const float y1 = r.y() + r.height();

        *vertex++ = { x0, y0 }; // Top-left
        *vertex++ = { x1, y0 }; // Top-right
        *vertex++ = { x1, y1 }; // Bottom-right
        *vertex++ = { x0, y1 }; // Bottom-left
    }

    const QRect r = region.boundingRect();

    const VkRect2D scissor = {
        .offset = { (int32_t)  r.x(),     (int32_t)  r.y()      },
        .extent = { (uint32_t) r.width(), (uint32_t) r.height() }
    };

    VkPipeline pipeline;
    VkPipelineLayout pipelineLayout;

    std::tie(pipeline, pipelineLayout) =
            pipelineManager()->pipeline(VulkanPipelineManager::FlatColor,
                                        VulkanPipelineManager::NoTraits,
                                        VulkanPipelineManager::DescriptorSet,
                                        VulkanPipelineManager::TriangleList,
                                        VulkanPipelineManager::SwapchainRenderPass);

    if (!m_clearDescriptorSet || m_clearDescriptorSet->uniformBuffer() != ubo.buffer()) {
        if (!m_clearDescriptorSet || m_clearDescriptorSet.use_count() > 1)
            m_clearDescriptorSet = std::make_shared<ColorDescriptorSet>(colorDescriptorPool());

        m_clearDescriptorSet->update(ubo.buffer());
    }

    auto cmd = mainCommandBuffer();

    cmd->bindPipeline(VK_PIPELINE_BIND_POINT_GRAPHICS, pipeline);
    cmd->bindDescriptorSets(VK_PIPELINE_BIND_POINT_GRAPHICS, pipelineLayout, 0, { m_clearDescriptorSet->handle() }, { (uint32_t) ubo.offset() });
    cmd->bindVertexBuffers(0, { vbo.handle() }, { vbo.offset() } );
    cmd->bindIndexBuffer(indexBufferForQuadCount(region.rectCount()), 0, VK_INDEX_TYPE_UINT16);
    cmd->setScissor(0, 1, &scissor);
    cmd->drawIndexed(region.rectCount() * 6, 1, 0, 0, 0);

    addBusyReference(m_clearDescriptorSet);
}


void VulkanScene::paintCursor()
{
    // Don't paint if we use a hardware cursor
    if (!kwinApp()->platform()->usesSoftwareCursor())
        return;

    // TODO
}


void VulkanScene::beginRenderPass(VulkanCommandBuffer *cmd)
{
    VkFramebuffer framebuffer = m_swapchain->currentFramebuffer();
    VkRenderPass renderPass;

    if (m_clearPending || m_surfaceLayout == VK_IMAGE_LAYOUT_UNDEFINED) {
        renderPass = m_clearRenderPass;
        m_clearPending = false;
    } else if (m_surfaceLayout == VK_IMAGE_LAYOUT_PRESENT_SRC_KHR) {
        if (m_separatePresentQueue) {
            renderPass = m_loadRenderPass;
            m_needQueueOwnershipTransfer = true;
        } else {
            renderPass = m_transitionRenderPass;
        }
    } else {
        assert(m_surfaceLayout == VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL);
        renderPass = m_loadRenderPass;
    }

    const VkClearColorValue clearColor = {
        .float32 = { 0.0f, 0.0f, 0.0f, 1.0f }
    };

    const VkClearValue clearValue = {
        .color = clearColor,
    };

    uint32_t width = m_swapchain->width();
    uint32_t height = m_swapchain->height();

    const VkRenderPassBeginInfo renderPassBeginInfo = {
        .sType = VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO,
        .pNext = nullptr,
        .renderPass = renderPass,
        .framebuffer = framebuffer,
        .renderArea = {
            .offset = { 0,     0      },
            .extent = { width, height }
        },
        .clearValueCount = 1,
        .pClearValues = &clearValue
    };

    const VkViewport viewport = {
        .x = 0.0f,
        .y = 0.0f,
        .width = float(width),
        .height = float(height),
        .minDepth = 0.0f,
        .maxDepth = 1.0f
    };

    // Begin recording the command buffer
    // ----------------------------------
    cmd->begin(VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT);

    if (m_needQueueOwnershipTransfer) {
        // Acquire graphics queue onwership of the swapchain image.
        // The present queue release operation is handled by paint().
        cmd->pipelineBarrier(VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT,
                             VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT,
                             0,
                             {},
                             {},
                             {
                                 {
                                     .sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER,
                                     .pNext = nullptr,
                                     .srcAccessMask = 0,
                                     .dstAccessMask = VK_ACCESS_COLOR_ATTACHMENT_READ_BIT |
                                                      VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT,
                                     .oldLayout = VK_IMAGE_LAYOUT_PRESENT_SRC_KHR,
                                     .newLayout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL,
                                     .srcQueueFamilyIndex = m_presentQueue.familyIndex(),
                                     .dstQueueFamilyIndex = m_graphicsQueue.familyIndex(),
                                     .image = m_swapchain->currentImage(),
                                     .subresourceRange = {
                                         .aspectMask = VK_IMAGE_ASPECT_COLOR_BIT,
                                         .baseMipLevel = 0,
                                         .levelCount = 1,
                                         .baseArrayLayer = 0,
                                         .layerCount = 1
                                     }
                                 }
                             });
    }

    cmd->beginRenderPass(&renderPassBeginInfo, VK_SUBPASS_CONTENTS_INLINE);
    cmd->setViewport(0, 1, &viewport);

    m_surfaceLayout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;
    m_renderPassStarted = true;
}


VulkanCommandBuffer *VulkanScene::mainCommandBuffer()
{
    auto &paintPass = currentPaintPass();
    auto cmd = currentPaintPass().mainCommandBuffer.get();

    if (!cmd->isActive()) {
        assert(!paintPass.fenceSubmitted);
        assert(!m_renderPassStarted);
        beginRenderPass(cmd);
        m_commandBuffersPending = true;
    }

    return cmd;
}


VulkanCommandBuffer *VulkanScene::setupCommandBuffer()
{
    auto &paintPass = currentPaintPass();
    auto cmd = paintPass.setupCommandBuffer.get();

    if (!cmd->isActive()) {
        if (paintPass.fenceSubmitted) {
            paintPass.fence.wait();
            paintPass.fenceSubmitted = false;

            // Reset the command pools
            for (auto &pool : paintPass.commandPools)
                pool.reset();

            // Drop busy references
            paintPass.busyObjects.clear();
        }

        cmd->begin(VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT);
        m_commandBuffersPending = true;
    }

    return cmd;
}


CompositingType VulkanScene::compositingType() const
{
    return VulkanCompositing;
}


bool VulkanScene::initFailed() const
{
    return !m_valid;
}


Scene::EffectFrame *VulkanScene::createEffectFrame(EffectFrameImpl *frame)
{
    return new VulkanEffectFrame(frame, this);
}


Shadow *VulkanScene::createShadow(Toplevel *toplevel)
{
    return new VulkanShadow(toplevel, this);
}


Decoration::Renderer *VulkanScene::createDecorationRenderer(Decoration::DecoratedClientImpl *impl)
{
    return new VulkanDecorationRenderer(impl, this);
}


void VulkanScene::screenGeometryChanged(const QSize &size)
{
    m_backend->screenGeometryChanged(size);

    if (m_swapchain)
        m_swapchain->invalidate();
}


Scene::Window *VulkanScene::createWindow(Toplevel *toplevel)
{
    return new VulkanWindow(toplevel, this);
}


bool VulkanScene::createRenderPasses()
{
    const VkAttachmentDescription loadBackBufferAttachment {
        .flags = 0,
        .format = m_surfaceFormat,
        .samples = VK_SAMPLE_COUNT_1_BIT,
        .loadOp = VK_ATTACHMENT_LOAD_OP_LOAD,
        .storeOp = VK_ATTACHMENT_STORE_OP_STORE,
        .stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE,
        .stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE,
        .initialLayout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL,
        .finalLayout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL
    };

    const VkAttachmentDescription transitionBackBufferAttachment {
        .flags = 0,
        .format = m_surfaceFormat,
        .samples = VK_SAMPLE_COUNT_1_BIT,
        .loadOp = VK_ATTACHMENT_LOAD_OP_LOAD,
        .storeOp = VK_ATTACHMENT_STORE_OP_STORE,
        .stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE,
        .stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE,
        .initialLayout = VK_IMAGE_LAYOUT_PRESENT_SRC_KHR,
        .finalLayout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL
    };

    const VkAttachmentDescription clearBackBufferAttachment {
        .flags = 0,
        .format = m_surfaceFormat,
        .samples = VK_SAMPLE_COUNT_1_BIT,
        .loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR,
        .storeOp = VK_ATTACHMENT_STORE_OP_STORE,
        .stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE,
        .stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE,
        .initialLayout = VK_IMAGE_LAYOUT_UNDEFINED,
        .finalLayout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL
    };

    static const VkAttachmentReference colorAttachmentRef {
        .attachment = 0,
        .layout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL
    };

    const VkSubpassDescription subpass {
        .flags = 0,
        .pipelineBindPoint = VK_PIPELINE_BIND_POINT_GRAPHICS,
        .inputAttachmentCount = 0,
        .pInputAttachments = nullptr,
        .colorAttachmentCount = 1,
        .pColorAttachments = &colorAttachmentRef,
        .pResolveAttachments = nullptr,
        .pDepthStencilAttachment = nullptr,
        .preserveAttachmentCount = 0,
        .pPreserveAttachments = nullptr
    };

    static const VkSubpassDependency clearStartDependency = {
        .srcSubpass = VK_SUBPASS_EXTERNAL,
        .dstSubpass = 0,
        .srcStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT,
        .dstStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT,
        .srcAccessMask = 0,
        .dstAccessMask = VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT,
        .dependencyFlags = 0
    };

    static const VkSubpassDependency transitionStartDependency = {
        .srcSubpass = VK_SUBPASS_EXTERNAL,
        .dstSubpass = 0,
        .srcStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT,
        .dstStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT,
        .srcAccessMask = 0,
        .dstAccessMask = VK_ACCESS_COLOR_ATTACHMENT_READ_BIT | VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT,
        .dependencyFlags = 0
    };

    static const VkSubpassDependency loadStartDependency = {
        .srcSubpass = VK_SUBPASS_EXTERNAL,
        .dstSubpass = 0,
        .srcStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT,
        .dstStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT,
        .srcAccessMask = 0,
        .dstAccessMask = VK_ACCESS_COLOR_ATTACHMENT_READ_BIT | VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT,
        .dependencyFlags = 0
    };

    static const VkSubpassDependency endDependency = {
        .srcSubpass = 0,
        .dstSubpass = VK_SUBPASS_EXTERNAL,
        .srcStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT,
        .dstStageMask = VK_PIPELINE_STAGE_BOTTOM_OF_PIPE_BIT,
        .srcAccessMask = VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT,
        .dstAccessMask = 0,
        .dependencyFlags = 0
    };

    m_clearRenderPass          = VulkanRenderPass(device(), { clearBackBufferAttachment      }, { subpass }, { clearStartDependency,      endDependency });
    m_transitionRenderPass     = VulkanRenderPass(device(), { transitionBackBufferAttachment }, { subpass }, { transitionStartDependency, endDependency });
    m_loadRenderPass           = VulkanRenderPass(device(), { loadBackBufferAttachment       }, { subpass }, { loadStartDependency,       endDependency });

    return true;
}


VkBuffer VulkanScene::indexBufferForQuadCount(uint32_t quadCount)
{
    if (m_indexBufferQuadCount >= quadCount) {
        if (!m_usedIndexBuffer) {
            addBusyReference(m_indexBuffer);
            addBusyReference(m_indexBufferMemory);

            m_usedIndexBuffer = true;
        }

        return m_indexBuffer->handle();
    }

    const size_t bytesPerQuad = 6 * sizeof(uint16_t);
    quadCount = std::max<uint32_t>(align(quadCount * bytesPerQuad, 4096), 16384) / bytesPerQuad;

    auto writeIndices = [](uint16_t *dst, uint32_t quadCount) {
        for (uint32_t i = 0; i < quadCount; i++) {
            for (int j : {1, 0, 3, 3, 2, 1}) {
                *dst++ = i * 4 + j;
            }
        }
    };

    size_t size = quadCount * bytesPerQuad;
    m_indexBuffer = std::make_shared<VulkanBuffer>(device(),
                                                   VkBufferCreateInfo {
                                                       .sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO,
                                                       .pNext = nullptr,
                                                       .flags = 0,
                                                       .size = size,
                                                       .usage = VK_BUFFER_USAGE_INDEX_BUFFER_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT,
                                                       .sharingMode = VK_SHARING_MODE_EXCLUSIVE,
                                                       .queueFamilyIndexCount = 0,
                                                       .pQueueFamilyIndices = nullptr,
                                                    });

    m_indexBufferMemory = deviceMemoryAllocator()->allocateMemory(m_indexBuffer, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT, 0);

    if (m_indexBufferMemory->isHostVisible()) {
        // Map the memory if it is host visible and write the indices directly into it
        uint16_t *indices = nullptr;
        m_indexBufferMemory->map(0, (void **) &indices);

        writeIndices(indices, quadCount);

        if (!m_indexBufferMemory->isHostCoherent()) {
            m_device->flushMappedMemoryRanges({
                                                  {
                                                      .sType = VK_STRUCTURE_TYPE_MAPPED_MEMORY_RANGE,
                                                      .pNext = nullptr,
                                                      .memory = m_indexBufferMemory->handle(),
                                                      .offset = 0,
                                                      .size = VK_WHOLE_SIZE
                                                  }
                                              });
        }

        m_indexBufferMemory->unmap();
    } else {
        // Upload the indices to staging memory and copy them to the index buffer
        auto range = uploadManager()->allocate(size);
        writeIndices((uint16_t *) range.data(), quadCount);
        auto cmd = setupCommandBuffer();

        cmd->copyBuffer(range.buffer()->handle(),
                        m_indexBuffer->handle(),
                        {
                            {
                                .srcOffset = range.offset(),
                                .dstOffset = 0,
                                .size = size
                            }
                        });

        const VkBufferMemoryBarrier barrier {
            .sType = VK_STRUCTURE_TYPE_BUFFER_MEMORY_BARRIER,
            .pNext = nullptr,
            .srcAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT,
            .dstAccessMask = VK_ACCESS_INDEX_READ_BIT,
            .srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED,
            .dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED,
            .buffer = m_indexBuffer->handle(),
            .offset = 0,
            .size = size
        };

        cmd->pipelineBarrier(VK_PIPELINE_STAGE_TRANSFER_BIT,
                             VK_PIPELINE_STAGE_VERTEX_INPUT_BIT,
                             0, {}, { barrier }, {});
    }

    addBusyReference(m_indexBuffer);
    addBusyReference(m_indexBufferMemory);

    m_indexBufferQuadCount = quadCount;
    m_usedIndexBuffer = true;

    return m_indexBuffer->handle();
}


} // namespace KWin

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

#include "kwinvulkanutils_funcs.h"

#define RESOLVE(name) \
    pfn ##name = (PFN_vk ##name) vkGetInstanceProcAddr(instance, "vk" #name)

namespace KWin
{

// VK_KHR_surface
PFN_vkDestroySurfaceKHR                       pfnDestroySurfaceKHR;
PFN_vkGetPhysicalDeviceSurfaceSupportKHR      pfnGetPhysicalDeviceSurfaceSupportKHR;
PFN_vkGetPhysicalDeviceSurfaceCapabilitiesKHR pfnGetPhysicalDeviceSurfaceCapabilitiesKHR;
PFN_vkGetPhysicalDeviceSurfaceFormatsKHR      pfnGetPhysicalDeviceSurfaceFormatsKHR;
PFN_vkGetPhysicalDeviceSurfacePresentModesKHR pfnGetPhysicalDeviceSurfacePresentModesKHR;

#ifdef VK_USE_PLATFORM_XCB_KHR
// VK_KHR_xcb_surface
PFN_vkCreateXcbSurfaceKHR                        pfnCreateXcbSurfaceKHR;
PFN_vkGetPhysicalDeviceXcbPresentationSupportKHR pfnGetPhysicalDeviceXcbPresentationSupportKHR;
#endif

#ifdef VK_USE_PLATFORM_WAYLAND_KHR
// VK_KHR_wayland_surface
PFN_vkCreateWaylandSurfaceKHR                        pfnCreateWaylandSurfaceKHR;
PFN_vkGetPhysicalDeviceWaylandPresentationSupportKHR pfnGetPhysicalDeviceWaylandPresentationSupportKHR;
#endif

#ifdef VK_USE_PLATFORM_ANDROID_KHR
// VK_KHR_android_surface
PFN_vkCreateAndroidSurfaceKHR pfnCreateAndroidSurfaceKHR;
#endif

// VK_KHR_swapchain
PFN_vkCreateSwapchainKHR    pfnCreateSwapchainKHR;
PFN_vkDestroySwapchainKHR   pfnDestroySwapchainKHR;
PFN_vkGetSwapchainImagesKHR pfnGetSwapchainImagesKHR;
PFN_vkAcquireNextImageKHR   pfnAcquireNextImageKHR;
PFN_vkQueuePresentKHR       pfnQueuePresentKHR;

// VK_KHR_maintenance1
PFN_vkTrimCommandPoolKHR pfnTrimCommandPoolKHR;

// VK_KHR_push_descriptor
PFN_vkCmdPushDescriptorSetKHR pfnCmdPushDescriptorSetKHR;

// VK_KHR_descriptor_update_template
PFN_vkCreateDescriptorUpdateTemplateKHR   pfnCreateDescriptorUpdateTemplateKHR;
PFN_vkDestroyDescriptorUpdateTemplateKHR  pfnDestroyDescriptorUpdateTemplateKHR;
PFN_vkUpdateDescriptorSetWithTemplateKHR  pfnUpdateDescriptorSetWithTemplateKHR;
PFN_vkCmdPushDescriptorSetWithTemplateKHR pfnCmdPushDescriptorSetWithTemplateKHR;

// VK_EXT_debug_report
PFN_vkCreateDebugReportCallbackEXT  pfnCreateDebugReportCallbackEXT;
PFN_vkDestroyDebugReportCallbackEXT pfnDestroyDebugReportCallbackEXT;
PFN_vkDebugReportMessageEXT         pfnDebugReportMessageEXT;

// VK_KHR_get_physical_device_properties2
PFN_vkGetPhysicalDeviceFeatures2KHR                    pfnGetPhysicalDeviceFeatures2KHR;
PFN_vkGetPhysicalDeviceProperties2KHR                  pfnGetPhysicalDeviceProperties2KHR;
PFN_vkGetPhysicalDeviceFormatProperties2KHR            pfnGetPhysicalDeviceFormatProperties2KHR;
PFN_vkGetPhysicalDeviceImageFormatProperties2KHR       pfnGetPhysicalDeviceImageFormatProperties2KHR;
PFN_vkGetPhysicalDeviceQueueFamilyProperties2KHR       pfnGetPhysicalDeviceQueueFamilyProperties2KHR;
PFN_vkGetPhysicalDeviceMemoryProperties2KHR            pfnGetPhysicalDeviceMemoryProperties2KHR;
PFN_vkGetPhysicalDeviceSparseImageFormatProperties2KHR pfnGetPhysicalDeviceSparseImageFormatProperties2KHR;

// VK_KHR_external_memory_capabilities
PFN_vkGetPhysicalDeviceExternalBufferPropertiesKHR pfnGetPhysicalDeviceExternalBufferPropertiesKHR;

// VK_KHR_external_memory_fd
PFN_vkGetMemoryFdKHR           pfnGetMemoryFdKHR;
PFN_vkGetMemoryFdPropertiesKHR pfnGetMemoryFdPropertiesKHR;

// VK_KHR_external_semaphore_capabilities
PFN_vkGetPhysicalDeviceExternalSemaphorePropertiesKHR pfnGetPhysicalDeviceExternalSemaphorePropertiesKHR;

// VK_KHR_external_semaphore_fd
PFN_vkImportSemaphoreFdKHR pfnImportSemaphoreFdKHR;
PFN_vkGetSemaphoreFdKHR    pfnGetSemaphoreFdKHR;

// VK_KHR_get_memory_requirements2
PFN_vkGetImageMemoryRequirements2KHR       pfnGetImageMemoryRequirements2KHR;
PFN_vkGetBufferMemoryRequirements2KHR      pfnGetBufferMemoryRequirements2KHR;
PFN_vkGetImageSparseMemoryRequirements2KHR pfnGetImageSparseMemoryRequirements2KHR;

// VK_EXT_discard_rectangles
PFN_vkCmdSetDiscardRectangleEXT pfnCmdSetDiscardRectangleEXT;

// VK_KHR_bind_memory2
PFN_vkBindBufferMemory2KHR pfnBindBufferMemory2KHR;
PFN_vkBindImageMemory2KHR pfnBindImageMemory2KHR;


void resolveFunctions(VkInstance instance)
{
    // VK_KHR_surface
    RESOLVE (DestroySurfaceKHR);
    RESOLVE (GetPhysicalDeviceSurfaceSupportKHR);
    RESOLVE (GetPhysicalDeviceSurfaceCapabilitiesKHR);
    RESOLVE (GetPhysicalDeviceSurfaceFormatsKHR);
    RESOLVE (GetPhysicalDeviceSurfacePresentModesKHR);

#ifdef VK_USE_PLATFORM_XCB_KHR
    // VK_KHR_xcb_surface
    RESOLVE (CreateXcbSurfaceKHR);
    RESOLVE (GetPhysicalDeviceXcbPresentationSupportKHR);
#endif

#ifdef VK_USE_PLATFORM_WAYLAND_KHR
    // VK_KHR_wayland_surface
    RESOLVE (CreateWaylandSurfaceKHR);
    RESOLVE (GetPhysicalDeviceWaylandPresentationSupportKHR);
#endif

#ifdef VK_USE_PLATFORM_ANDROID_KHR
    // VK_KHR_android_surface
    RESOLVE (CreateAndroidSurfaceKHR);
#endif

    // VK_KHR_swapchain
    RESOLVE (CreateSwapchainKHR);
    RESOLVE (DestroySwapchainKHR);
    RESOLVE (GetSwapchainImagesKHR);
    RESOLVE (AcquireNextImageKHR);
    RESOLVE (QueuePresentKHR);

    // VK_KHR_maintenance1
    RESOLVE (TrimCommandPoolKHR);

    // VK_KHR_push_descriptor
    RESOLVE (CmdPushDescriptorSetKHR);

    // VK_KHR_descriptor_update_template
    RESOLVE (CreateDescriptorUpdateTemplateKHR);
    RESOLVE (DestroyDescriptorUpdateTemplateKHR);
    RESOLVE (UpdateDescriptorSetWithTemplateKHR);
    RESOLVE (CmdPushDescriptorSetWithTemplateKHR);

    // VK_EXT_debug_report
    RESOLVE (CreateDebugReportCallbackEXT);
    RESOLVE (CreateDebugReportCallbackEXT);
    RESOLVE (DestroyDebugReportCallbackEXT);
    RESOLVE (DebugReportMessageEXT);

    // VK_KHR_get_physical_device_properties2
    RESOLVE (GetPhysicalDeviceFeatures2KHR);
    RESOLVE (GetPhysicalDeviceProperties2KHR);
    RESOLVE (GetPhysicalDeviceFormatProperties2KHR);
    RESOLVE (GetPhysicalDeviceImageFormatProperties2KHR);
    RESOLVE (GetPhysicalDeviceQueueFamilyProperties2KHR);
    RESOLVE (GetPhysicalDeviceMemoryProperties2KHR);
    RESOLVE (GetPhysicalDeviceSparseImageFormatProperties2KHR);

    // VK_KHR_external_memory_capabilities
    RESOLVE (GetPhysicalDeviceExternalBufferPropertiesKHR);

    // VK_KHR_external_memory_fd
    RESOLVE (GetMemoryFdKHR);
    RESOLVE (GetMemoryFdPropertiesKHR);

    // VK_KHR_external_semaphore_capabilities
    RESOLVE (GetPhysicalDeviceExternalSemaphorePropertiesKHR);

    // VK_KHR_external_semaphore_fd
    RESOLVE (ImportSemaphoreFdKHR);
    RESOLVE (GetSemaphoreFdKHR);

    // VK_KHR_get_memory_requirements2
    RESOLVE (GetImageMemoryRequirements2KHR);
    RESOLVE (GetBufferMemoryRequirements2KHR);
    RESOLVE (GetImageSparseMemoryRequirements2KHR);

    // VK_EXT_discard_rectangles
    RESOLVE (CmdSetDiscardRectangleEXT);

    // VK_KHR_bind_memory2
    RESOLVE (BindBufferMemory2KHR);
    RESOLVE (BindImageMemory2KHR);
}

} // namespace KWin

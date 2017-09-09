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

#ifndef KWINVULKANUTILS_FUNCS_H
#define KWINVULKANUTILS_FUNCS_H

#include <kwinvulkanutils_export.h>

#define VK_USE_PLATFORM_XCB_KHR
#define VK_USE_PLATFORM_WAYLAND_KHR
#include <vulkan/vulkan.h>

namespace KWin
{

class VulkanExtensionPropertyList;

// VK_KHR_surface
extern KWINVULKANUTILS_EXPORT PFN_vkDestroySurfaceKHR                       pfnDestroySurfaceKHR;
extern KWINVULKANUTILS_EXPORT PFN_vkGetPhysicalDeviceSurfaceSupportKHR      pfnGetPhysicalDeviceSurfaceSupportKHR;
extern KWINVULKANUTILS_EXPORT PFN_vkGetPhysicalDeviceSurfaceCapabilitiesKHR pfnGetPhysicalDeviceSurfaceCapabilitiesKHR;
extern KWINVULKANUTILS_EXPORT PFN_vkGetPhysicalDeviceSurfaceFormatsKHR      pfnGetPhysicalDeviceSurfaceFormatsKHR;
extern KWINVULKANUTILS_EXPORT PFN_vkGetPhysicalDeviceSurfacePresentModesKHR pfnGetPhysicalDeviceSurfacePresentModesKHR;

#ifdef VK_USE_PLATFORM_XCB_KHR
// VK_KHR_xcb_surface
extern KWINVULKANUTILS_EXPORT PFN_vkCreateXcbSurfaceKHR                        pfnCreateXcbSurfaceKHR;
extern KWINVULKANUTILS_EXPORT PFN_vkGetPhysicalDeviceXcbPresentationSupportKHR pfnGetPhysicalDeviceXcbPresentationSupportKHR;
#endif

#ifdef VK_USE_PLATFORM_WAYLAND_KHR
// VK_KHR_wayland_surface
extern KWINVULKANUTILS_EXPORT PFN_vkCreateWaylandSurfaceKHR                        pfnCreateWaylandSurfaceKHR;
extern KWINVULKANUTILS_EXPORT PFN_vkGetPhysicalDeviceWaylandPresentationSupportKHR pfnGetPhysicalDeviceWaylandPresentationSupportKHR;
#endif

#ifdef VK_USE_PLATFORM_ANDROID_KHR
// VK_KHR_android_surface
extern KWINVULKANUTILS_EXPORT PFN_vkCreateAndroidSurfaceKHR pfnCreateAndroidSurfaceKHR;
#endif

// VK_KHR_swapchain
extern KWINVULKANUTILS_EXPORT PFN_vkCreateSwapchainKHR    pfnCreateSwapchainKHR;
extern KWINVULKANUTILS_EXPORT PFN_vkDestroySwapchainKHR   pfnDestroySwapchainKHR;
extern KWINVULKANUTILS_EXPORT PFN_vkGetSwapchainImagesKHR pfnGetSwapchainImagesKHR;
extern KWINVULKANUTILS_EXPORT PFN_vkAcquireNextImageKHR   pfnAcquireNextImageKHR;
extern KWINVULKANUTILS_EXPORT PFN_vkQueuePresentKHR       pfnQueuePresentKHR;

// VK_KHR_maintenance1
extern KWINVULKANUTILS_EXPORT PFN_vkTrimCommandPoolKHR pfnTrimCommandPoolKHR;

// VK_KHR_push_descriptor
extern KWINVULKANUTILS_EXPORT PFN_vkCmdPushDescriptorSetKHR pfnCmdPushDescriptorSetKHR;

// VK_KHR_descriptor_update_template
extern KWINVULKANUTILS_EXPORT PFN_vkCreateDescriptorUpdateTemplateKHR   pfnCreateDescriptorUpdateTemplateKHR;
extern KWINVULKANUTILS_EXPORT PFN_vkDestroyDescriptorUpdateTemplateKHR  pfnDestroyDescriptorUpdateTemplateKHR;
extern KWINVULKANUTILS_EXPORT PFN_vkUpdateDescriptorSetWithTemplateKHR  pfnUpdateDescriptorSetWithTemplateKHR;
extern KWINVULKANUTILS_EXPORT PFN_vkCmdPushDescriptorSetWithTemplateKHR pfnCmdPushDescriptorSetWithTemplateKHR;

// VK_EXT_debug_report
extern KWINVULKANUTILS_EXPORT PFN_vkCreateDebugReportCallbackEXT  pfnCreateDebugReportCallbackEXT;
extern KWINVULKANUTILS_EXPORT PFN_vkDestroyDebugReportCallbackEXT pfnDestroyDebugReportCallbackEXT;
extern KWINVULKANUTILS_EXPORT PFN_vkDebugReportMessageEXT         pfnDebugReportMessageEXT;

// VK_KHR_get_physical_device_properties2
extern KWINVULKANUTILS_EXPORT PFN_vkGetPhysicalDeviceFeatures2KHR                    pfnGetPhysicalDeviceFeatures2KHR;
extern KWINVULKANUTILS_EXPORT PFN_vkGetPhysicalDeviceProperties2KHR                  pfnGetPhysicalDeviceProperties2KHR;
extern KWINVULKANUTILS_EXPORT PFN_vkGetPhysicalDeviceFormatProperties2KHR            pfnGetPhysicalDeviceFormatProperties2KHR;
extern KWINVULKANUTILS_EXPORT PFN_vkGetPhysicalDeviceImageFormatProperties2KHR       pfnGetPhysicalDeviceImageFormatProperties2KHR;
extern KWINVULKANUTILS_EXPORT PFN_vkGetPhysicalDeviceQueueFamilyProperties2KHR       pfnGetPhysicalDeviceQueueFamilyProperties2KHR;
extern KWINVULKANUTILS_EXPORT PFN_vkGetPhysicalDeviceMemoryProperties2KHR            pfnGetPhysicalDeviceMemoryProperties2KHR;
extern KWINVULKANUTILS_EXPORT PFN_vkGetPhysicalDeviceSparseImageFormatProperties2KHR pfnGetPhysicalDeviceSparseImageFormatProperties2KHR;

// VK_KHR_external_memory_capabilities
extern KWINVULKANUTILS_EXPORT PFN_vkGetPhysicalDeviceExternalBufferPropertiesKHR pfnGetPhysicalDeviceExternalBufferPropertiesKHR;

// VK_KHR_external_memory_fd
extern KWINVULKANUTILS_EXPORT PFN_vkGetMemoryFdKHR           pfnGetMemoryFdKHR;
extern KWINVULKANUTILS_EXPORT PFN_vkGetMemoryFdPropertiesKHR pfnGetMemoryFdPropertiesKHR;

// VK_KHR_external_semaphore_capabilities
extern KWINVULKANUTILS_EXPORT PFN_vkGetPhysicalDeviceExternalSemaphorePropertiesKHR pfnGetPhysicalDeviceExternalSemaphorePropertiesKHR;

// VK_KHR_external_semaphore_fd
extern KWINVULKANUTILS_EXPORT PFN_vkImportSemaphoreFdKHR pfnImportSemaphoreFdKHR;
extern KWINVULKANUTILS_EXPORT PFN_vkGetSemaphoreFdKHR    pfnGetSemaphoreFdKHR;

// VK_KHR_get_memory_requirements2
extern KWINVULKANUTILS_EXPORT PFN_vkGetImageMemoryRequirements2KHR       pfnGetImageMemoryRequirements2KHR;
extern KWINVULKANUTILS_EXPORT PFN_vkGetBufferMemoryRequirements2KHR      pfnGetBufferMemoryRequirements2KHR;
extern KWINVULKANUTILS_EXPORT PFN_vkGetImageSparseMemoryRequirements2KHR pfnGetImageSparseMemoryRequirements2KHR;

// VK_EXT_discard_rectangles
extern KWINVULKANUTILS_EXPORT PFN_vkCmdSetDiscardRectangleEXT pfnCmdSetDiscardRectangleEXT;

// VK_KHR_bind_memory2
extern KWINVULKANUTILS_EXPORT PFN_vkBindBufferMemory2KHR pfnBindBufferMemory2KHR;
extern KWINVULKANUTILS_EXPORT PFN_vkBindImageMemory2KHR pfnBindImageMemory2KHR;

void KWINVULKANUTILS_EXPORT resolveFunctions(VkInstance instance);

} // namespace KWin

#endif // KWINVULKANUTILS_FUNCS_H


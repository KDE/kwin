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

#include "swapchain.h"
#include "screens.h"
#include "logging.h"

namespace KWin
{

template <typename T>
constexpr const T &clamp(const T &v, const T &lo, const T &hi)
{
    return std::max(lo, std::min(hi, v));
}


VulkanSwapchain::VulkanSwapchain(VulkanDevice *device,
                                 VulkanQueue queue,
                                 VkSurfaceKHR surface,
                                 VkFormat format,
                                 VkColorSpaceKHR colorSpace,
                                 VkPresentModeKHR presentMode,
                                 VkRenderPass renderPass)
    : m_device(device),
      m_queue(queue),
      m_surface(surface),
      m_format(format),
      m_colorSpace(colorSpace),
      m_presentMode(presentMode),
      m_preferredImageCount(2),
      m_renderPass(renderPass),
      m_swapchain(VK_NULL_HANDLE),
      m_extent({0, 0}),
      m_isValid(false)
{
}


VulkanSwapchain::~VulkanSwapchain()
{
    // Destroy the old image views and framebuffers
    for (Buffer &buffer : m_buffers) {
        if (buffer.framebuffer)
            m_device->destroyFramebuffer(buffer.framebuffer, nullptr);

        if (buffer.imageView)
            m_device->destroyImageView(buffer.imageView, nullptr);
    }

    m_device->destroySwapchainKHR(m_swapchain, nullptr);
}


VkResult VulkanSwapchain::create()
{
    m_isValid = false;

    auto physicalDevice = m_device->physicalDevice();

    VkSurfaceCapabilitiesKHR capabilities;
    VkResult result = physicalDevice.getSurfaceCapabilitiesKHR(m_surface, &capabilities);

    if (result != VK_SUCCESS) {
        qCCritical(KWIN_VULKAN) << "vkPhysicalDeviceGetSurfaceCapabilitiesKHR returned" << enumToString(result);
        return result;
    }

    if (capabilities.currentExtent.width == UINT32_MAX) {
        const QSize size = Screens::self()->size();
        m_extent.width  = clamp<uint32_t>(size.width(),  capabilities.minImageExtent.width,  capabilities.maxImageExtent.width);
        m_extent.height = clamp<uint32_t>(size.height(), capabilities.minImageExtent.height, capabilities.maxImageExtent.height);
    } else {
        m_extent = capabilities.currentExtent;
    }

    uint32_t imageCount = std::max<uint32_t>(capabilities.minImageCount, m_preferredImageCount);
    if (capabilities.maxImageCount > 0)
        imageCount = std::min(imageCount, capabilities.maxImageCount);

    VkSurfaceTransformFlagBitsKHR preTransform;
    if (capabilities.supportedTransforms & VK_SURFACE_TRANSFORM_IDENTITY_BIT_KHR)
        preTransform = VK_SURFACE_TRANSFORM_IDENTITY_BIT_KHR;
    else
        preTransform = capabilities.currentTransform;

    VkCompositeAlphaFlagBitsKHR compositeAlpha;

    if (capabilities.supportedCompositeAlpha & VK_COMPOSITE_ALPHA_OPAQUE_BIT_KHR)
        compositeAlpha = VK_COMPOSITE_ALPHA_OPAQUE_BIT_KHR;
    else if (capabilities.supportedCompositeAlpha & VK_COMPOSITE_ALPHA_INHERIT_BIT_KHR)
        compositeAlpha = VK_COMPOSITE_ALPHA_INHERIT_BIT_KHR;
    else if (capabilities.supportedCompositeAlpha & VK_COMPOSITE_ALPHA_PRE_MULTIPLIED_BIT_KHR)
        compositeAlpha = VK_COMPOSITE_ALPHA_PRE_MULTIPLIED_BIT_KHR;
    else
        compositeAlpha = VK_COMPOSITE_ALPHA_POST_MULTIPLIED_BIT_KHR;

    VkImageUsageFlags imageUsage = VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT | VK_IMAGE_USAGE_TRANSFER_SRC_BIT;
    imageUsage &= capabilities.supportedUsageFlags;

    VkSwapchainKHR oldSwapchain = m_swapchain;

    const VkSwapchainCreateInfoKHR createInfo = {
        .sType = VK_STRUCTURE_TYPE_SWAPCHAIN_CREATE_INFO_KHR,
        .pNext = nullptr,
        .flags = 0,
        .surface = m_surface,
        .minImageCount = imageCount,
        .imageFormat = m_format,
        .imageColorSpace = m_colorSpace,
        .imageExtent = m_extent,
        .imageArrayLayers = 1,
        .imageUsage = imageUsage,
        .imageSharingMode = VK_SHARING_MODE_EXCLUSIVE,
        .queueFamilyIndexCount = 0,
        .pQueueFamilyIndices = nullptr,
        .preTransform = preTransform,
        .compositeAlpha = compositeAlpha,
        .presentMode = m_presentMode,
        .clipped = VK_FALSE,
        .oldSwapchain = oldSwapchain
    };

    result = m_device->createSwapchainKHR(&createInfo, nullptr, &m_swapchain);

    if (result != VK_SUCCESS) {
        qCCritical(KWIN_VULKAN) << "vkCreateSwapchainKHR returned" << enumToString(result);
        return result;
    }

    m_queue.waitIdle();

    // Destroy the old swap chain now if we are recreating it
    if (oldSwapchain != VK_NULL_HANDLE) {
        m_device->destroySwapchainKHR(oldSwapchain, nullptr);
    }

    // Destroy the old image views, framebuffers and command buffers
    for (Buffer &buffer : m_buffers) {
        if (buffer.framebuffer)
            m_device->destroyFramebuffer(buffer.framebuffer, nullptr);

        if (buffer.imageView)
            m_device->destroyImageView(buffer.imageView, nullptr);

        buffer.acquireOwnershipCommandBuffer = nullptr;
        buffer.releaseOwnershipCommandBuffer = nullptr;
    }

    // Get the actual swapchain image count
    if ((result = m_device->getSwapchainImagesKHR(m_swapchain, &imageCount, nullptr)) != VK_SUCCESS) {
        qCCritical(KWIN_VULKAN) << "vkGetSwapchainImagesKHR returned" << enumToString(result);
        return result;
    }

    // Get the swapchain images
    std::vector<VkImage> images(imageCount);
    if ((result = m_device->getSwapchainImagesKHR(m_swapchain, &imageCount, images.data())) != VK_SUCCESS) {
        qCCritical(KWIN_VULKAN) << "vkGetSwapchainImagesKHR returned" << enumToString(result);
        return result;
    }

    m_buffers.resize(images.size());

    // Create the image views and framebuffers
    for (unsigned int i = 0; i < m_buffers.size(); i++) {
        m_buffers[i].image = images[i];
        m_buffers[i].imageView = VK_NULL_HANDLE;
        m_buffers[i].framebuffer = VK_NULL_HANDLE;
        m_buffers[i].sequence = 0;

        const VkImageViewCreateInfo imageViewCreateInfo = {
            .sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO,
            .pNext = nullptr,
            .flags = 0,
            .image = images[i],
            .viewType = VK_IMAGE_VIEW_TYPE_2D,
            .format = m_format,
            .components = {
                .r = VK_COMPONENT_SWIZZLE_IDENTITY,
                .g = VK_COMPONENT_SWIZZLE_IDENTITY,
                .b = VK_COMPONENT_SWIZZLE_IDENTITY,
                .a = VK_COMPONENT_SWIZZLE_IDENTITY
            },
            .subresourceRange = {
                .aspectMask = VK_IMAGE_ASPECT_COLOR_BIT,
                .baseMipLevel = 0,
                .levelCount = 1,
                .baseArrayLayer = 0,
                .layerCount = 1
            }
        };

        if ((result = m_device->createImageView(&imageViewCreateInfo, nullptr, &m_buffers[i].imageView)) != VK_SUCCESS) {
            qCCritical(KWIN_VULKAN) << "vkCreateImageView returned" << enumToString(result);
            return result;
        }

        const VkFramebufferCreateInfo framebufferCreateInfo = {
            .sType = VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO,
            .pNext = nullptr,
            .flags = 0,
            .renderPass = m_renderPass,
            .attachmentCount = 1,
            .pAttachments = &m_buffers[i].imageView,
            .width = m_extent.width,
            .height = m_extent.height,
            .layers = 1
        };

        if ((result = m_device->createFramebuffer(&framebufferCreateInfo, nullptr, &m_buffers[i].framebuffer)) != VK_SUCCESS) {
            qCCritical(KWIN_VULKAN) << "vkCreateFramebuffer returned" << enumToString(result);
            return result;
        }
    }

    m_damageHistory.clear();
    m_isValid = true;
    m_sequence = 0;
    m_index = -1;

    return VK_SUCCESS;
}


VkResult VulkanSwapchain::acquireNextImage(uint64_t timeout, VkSemaphore semaphore, VkFence fence, int32_t *imageIndex)
{
    ++m_sequence;

    if (m_index != -1)
        m_buffers[m_index].sequence = m_sequence;

    VkResult result = m_device->acquireNextImageKHR(m_swapchain, timeout, semaphore, fence, (uint32_t *) &m_index);

    m_isValid = (result == VK_SUCCESS || result == VK_SUBOPTIMAL_KHR);

    if (imageIndex && m_isValid)
        *imageIndex = m_index;

    return result;
}


VkResult VulkanSwapchain::present(uint32_t imageIndex, const QRegion &updateRegion, VkSemaphore waitSemaphore)
{
    std::vector<VkRectLayerKHR> rects;

    if (m_supportsIncrementalPresent) {
        rects.reserve(updateRegion.rectCount());

        for (const QRect &r : updateRegion) {
            rects.emplace_back(VkRectLayerKHR {
                                   .offset = { r.x(),                 r.y()                 },
                                   .extent = { (uint32_t) r.width(),  (uint32_t) r.height() },
                                   .layer  = 0
                               });

        }
    }

    const VkPresentRegionKHR region = {
        .rectangleCount = (uint32_t) rects.size(),
        .pRectangles = rects.data()
    };

    const VkPresentRegionsKHR regions = {
        .sType = VK_STRUCTURE_TYPE_PRESENT_REGIONS_KHR,
        .pNext = nullptr,
        .swapchainCount = 1,
        .pRegions = &region
    };

    const VkPresentInfoKHR presentInfo = {
        .sType = VK_STRUCTURE_TYPE_PRESENT_INFO_KHR,
        .pNext = m_supportsIncrementalPresent ? &regions : nullptr,
        .waitSemaphoreCount = waitSemaphore ? 1u : 0u,
        .pWaitSemaphores = &waitSemaphore,
        .swapchainCount = 1,
        .pSwapchains = &m_swapchain,
        .pImageIndices = &imageIndex,
        .pResults = nullptr
    };

    VkResult result = m_queue.presentKHR(&presentInfo);

    if (result != VK_SUCCESS)
        m_isValid = false;

    return result;
}


uint32_t VulkanSwapchain::bufferAge() const
{
    if (m_index != -1 && m_buffers[m_index].sequence != 0)
        return m_sequence - m_buffers[m_index].sequence + 1;

    return 0;
}


void VulkanSwapchain::addToDamageHistory(const QRegion &region)
{
    if (m_damageHistory.size() > 10)
        m_damageHistory.pop_back();

    m_damageHistory.emplace_front(region);
}


QRegion VulkanSwapchain::accumulatedDamageHistory(int bufferAge) const
{
    QRegion region;

    // Note: An age of zero means the buffer contents are undefined
    if (bufferAge > 0 && bufferAge <= (int) m_damageHistory.size()) {
        auto it = m_damageHistory.cbegin();
        for (int i = 0; i < bufferAge - 1; i++)
            region |= *it++;
    } else {
        const QSize &s = screens()->size();
        region = QRegion(0, 0, s.width(), s.height());
    }

    return region;
}

} // namespace KWin

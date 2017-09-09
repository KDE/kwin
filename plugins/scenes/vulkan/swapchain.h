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

#ifndef SWAPCHAIN_H
#define SWAPCHAIN_H

#include "scene.h"

namespace KWin
{

class VulkanSwapchain
{
    struct Buffer {
        VkImage image;
        VkImageView imageView;
        VkFramebuffer framebuffer;
        std::shared_ptr<VulkanCommandBuffer> acquireOwnershipCommandBuffer;
        std::shared_ptr<VulkanCommandBuffer> releaseOwnershipCommandBuffer;
        uint64_t sequence;
    };

public:
    /**
     * Creates a Vulkan swap chain.
     *
     * @param device        The logical device
     * @param surface       The surface the swap chain will present to
     * @param format        The format the swap chain images will have
     * @param colorspace    The color space the swap chain images will use
     * @param renderPass    The render pass the swap chain images will be compatible with
     */
    VulkanSwapchain(VulkanDevice *device,
                    VulkanQueue queue,
                    VkSurfaceKHR surface,
                    VkFormat format,
                    VkColorSpaceKHR colorspace,
                    VkPresentModeKHR presentMode,
                    VkRenderPass renderPass);

    /**
     * Destroys the swap chain.
     */
    ~VulkanSwapchain();

    /**
     * (Re)creates the swap chain.
     *
     * This function must be called after the surface has been resized.
     */
    VkResult create();

    /**
     * Sets whether the implementation supports VK_KHR_incremental_present or not.
     */
    void setSupportsIncrementalPresent(bool value) { m_supportsIncrementalPresent = value; }

    /**
     * Sets the preferred number of images in the swap chain.
     */
    void setPreferredImageCount(int imageCount) { m_preferredImageCount = imageCount; }

    /**
     * Returns true if the swap chain is valid, and false otherwise.
     */
    bool isValid() const { return m_isValid; }

    /**
     * Marks the swapchain as invalid.
     */
    void invalidate() { m_isValid = false; }

    /**
     * Acquires a new image from the swap chain.
     */
    VkResult acquireNextImage(uint64_t timeout, VkSemaphore semaphore, VkFence fence, int32_t *imageIndex);

    VkResult present(uint32_t imageIndex, const QRegion &updateRegion, VkSemaphore waitSemaphore);

    /**
     * Returns the index of the current image, or -1 if no image has been acquired.
     */
    int32_t currentIndex() const { return m_index; }

    VkImage image(int32_t index) const { return m_buffers[index].image; }
    VkImageView imageView(int32_t index) const { return m_buffers[index].imageView; }
    VkFramebuffer framebuffer(int32_t index) const { return m_buffers[index].framebuffer; }

    VkImage currentImage() const { return image(m_index); }
    VkImageView currentImageView() const { return imageView(m_index); }
    VkFramebuffer currentFramebuffer() const { return framebuffer(m_index); }

    std::shared_ptr<VulkanCommandBuffer> &acquireOwnershipCommandBuffer() { return m_buffers[m_index].acquireOwnershipCommandBuffer; }
    std::shared_ptr<VulkanCommandBuffer> &releaseOwnershipCommandBuffer() { return m_buffers[m_index].releaseOwnershipCommandBuffer; }

    /**
     * Returns the width of the swapchain images.
     */
    uint32_t width() const { return m_extent.width; }

    /**
     * Returns the height of the swapchain images.
     */
    uint32_t height() const { return m_extent.height; }

    /**
     * Returns the extent of the swapchain images.
     */
    const VkExtent2D &extent() const { return m_extent; }

    /**
     * Returns the age of the current image.
     *
     * The age is the number of images that have been presented since
     * the current image was last acquired.
     *
     * An age of zero means that the image contents are undefined.
     */
    uint32_t bufferAge() const;

    /**
     * Adds the given damage region to the damage history.
     */
    void addToDamageHistory(const QRegion &region);

    /**
     * Returns the union of the damage regions for the last bufferAge frames.
     */
    QRegion accumulatedDamageHistory(int bufferAge) const;

private:
    VulkanDevice *m_device;
    VulkanQueue m_queue;
    VkSurfaceKHR m_surface;
    VkFormat m_format;
    VkColorSpaceKHR m_colorSpace;
    VkPresentModeKHR m_presentMode;
    uint32_t m_preferredImageCount;
    VkRenderPass m_renderPass;
    VkSwapchainKHR m_swapchain;
    VkExtent2D m_extent;
    std::vector<Buffer> m_buffers;
    std::deque<QRegion> m_damageHistory;
    int32_t m_index = -1;
    uint64_t m_sequence = 0;
    bool m_supportsIncrementalPresent = false;
    bool m_isValid = false;
};

} // namespace KWin

#endif // SWAPCHAIN_H

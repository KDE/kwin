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

#ifndef DESCRIPTOR_SET_H
#define DESCRIPTOR_SET_H

#include "kwinvulkanutils.h"

#include <deque>
#include <queue>


namespace KWin {


class SceneDescriptorPool;


/**
 * TextureDescriptorSet encapsulates a descriptor set with a combined image-sampler
 * and a dynamic uniform buffer binding.
 */
 class TextureDescriptorSet : public VulkanObject
{
public:
    TextureDescriptorSet(SceneDescriptorPool *pool);

    TextureDescriptorSet(const TextureDescriptorSet &other) = delete;

    TextureDescriptorSet(TextureDescriptorSet &&other)
        : m_pool(other.m_pool),
          m_set(other.m_set),
          m_sampler(other.m_sampler),
          m_imageView(other.m_imageView),
          m_imageLayout(other.m_imageLayout),
          m_uniformBuffer(std::move(other.m_uniformBuffer))
    {
        other.m_pool = nullptr;
        other.m_set = VK_NULL_HANDLE;
        other.m_sampler = VK_NULL_HANDLE;
        other.m_imageView = nullptr;
        other.m_imageLayout = VK_IMAGE_LAYOUT_UNDEFINED;
        other.m_uniformBuffer = nullptr;
    }

    ~TextureDescriptorSet() override;

    void update(VkSampler sampler,
                const std::shared_ptr<VulkanImageView> &imageView,
                VkImageLayout imageLayout,
                const std::shared_ptr<VulkanBuffer> &uniformBuffer);

    VkSampler sampler() const { return m_sampler; }
    std::shared_ptr<VulkanImageView> imageView() const { return m_imageView; }
    VkImageLayout imageLayout() const { return m_imageLayout; }
    std::shared_ptr<VulkanBuffer> uniformBuffer() const { return m_uniformBuffer; }

    VkDescriptorSet handle() const { return m_set; }
    operator VkDescriptorSet () const { return m_set; }

    TextureDescriptorSet &operator = (const TextureDescriptorSet &other) = delete;

    TextureDescriptorSet &operator = (TextureDescriptorSet &&other) {
        m_pool = other.m_pool;
        m_set = other.m_set;
        m_sampler = other.m_sampler;
        m_imageView = other.m_imageView;
        m_imageLayout = other.m_imageLayout;
        m_uniformBuffer = std::move(other.m_uniformBuffer);

        other.m_pool = nullptr;
        other.m_set = VK_NULL_HANDLE;
        other.m_sampler = VK_NULL_HANDLE;
        other.m_imageView = nullptr;
        other.m_imageLayout = VK_IMAGE_LAYOUT_UNDEFINED;
        other.m_uniformBuffer = nullptr;
        return *this;
    }

private:
    SceneDescriptorPool *m_pool;
    VkDescriptorSet m_set;
    VkSampler m_sampler;
    std::shared_ptr<VulkanImageView> m_imageView;
    VkImageLayout m_imageLayout;
    std::shared_ptr<VulkanBuffer> m_uniformBuffer;
};



// ------------------------------------------------------------------



/**
 * DecorationImagesDescriptorSet encapsulates a descriptor set with an array of four
 * sampled-images, and an immutable nearest-neighbor sampler.
 *
 * Note that this descriptor set does not have a uniform buffer binding.
 */
class DecorationImagesDescriptorSet : public VulkanObject
{
public:
    DecorationImagesDescriptorSet(SceneDescriptorPool *pool);

    DecorationImagesDescriptorSet(const DecorationImagesDescriptorSet &other) = delete;

    DecorationImagesDescriptorSet(DecorationImagesDescriptorSet &&other)
        : m_pool(other.m_pool),
          m_set(other.m_set)
    {
        other.m_pool = nullptr;
        other.m_set = VK_NULL_HANDLE;
    }

    ~DecorationImagesDescriptorSet() override;

    void update(const std::shared_ptr<VulkanImageView> &imageView1,
                const std::shared_ptr<VulkanImageView> &imageView2,
                const std::shared_ptr<VulkanImageView> &imageView3,
                const std::shared_ptr<VulkanImageView> &imageView4,
                VkImageLayout imageLayout);

    VkDescriptorSet handle() const { return m_set; }
    operator VkDescriptorSet () const { return m_set; }

    DecorationImagesDescriptorSet &operator = (const DecorationImagesDescriptorSet &other) = delete;

    DecorationImagesDescriptorSet &operator = (DecorationImagesDescriptorSet &&other) {
        m_pool = other.m_pool;
        m_set = other.m_set;

        other.m_pool = nullptr;
        other.m_set = VK_NULL_HANDLE;
        return *this;
    }

private:
    SceneDescriptorPool *m_pool;
    VkDescriptorSet m_set;
};



// ------------------------------------------------------------------



/**
 * CrossFadeDescriptorSet encapsulates a descriptor set with an array of two sampled-images,
 * and a dynamic uniform buffer binding.
 */
class CrossFadeDescriptorSet : public VulkanObject
{
public:
    CrossFadeDescriptorSet(SceneDescriptorPool *pool);

    CrossFadeDescriptorSet(const CrossFadeDescriptorSet &other) = delete;

    CrossFadeDescriptorSet(CrossFadeDescriptorSet &&other)
        : m_pool(other.m_pool),
          m_set(other.m_set),
          m_sampler(other.m_sampler),
          m_imageView1(other.m_imageView1),
          m_imageView2(other.m_imageView2),
          m_imageLayout(other.m_imageLayout),
          m_uniformBuffer(std::move(other.m_uniformBuffer))
    {
        other.m_pool = nullptr;
        other.m_set = VK_NULL_HANDLE;
        other.m_sampler = VK_NULL_HANDLE;
        other.m_imageView1 = nullptr;
        other.m_imageView2 = nullptr;
        other.m_imageLayout = VK_IMAGE_LAYOUT_UNDEFINED;
        other.m_uniformBuffer = nullptr;
    }

    ~CrossFadeDescriptorSet() override;

    void update(VkSampler sampler,
                const std::shared_ptr<VulkanImageView> &imageView1,
                const std::shared_ptr<VulkanImageView> &imageView2,
                VkImageLayout imageLayout,
                const std::shared_ptr<VulkanBuffer> &uniformBuffer);

    VkSampler sampler() const { return m_sampler; }
    std::shared_ptr<VulkanImageView> imageView1() const { return m_imageView1; }
    std::shared_ptr<VulkanImageView> imageView2() const { return m_imageView2; }
    VkImageLayout imageLayout() const { return m_imageLayout; }
    std::shared_ptr<VulkanBuffer> uniformBuffer() const { return m_uniformBuffer; }

    VkDescriptorSet handle() const { return m_set; }
    operator VkDescriptorSet () const { return m_set; }

    CrossFadeDescriptorSet &operator = (const CrossFadeDescriptorSet &other) = delete;

    CrossFadeDescriptorSet &operator = (CrossFadeDescriptorSet &&other) {
        m_pool = other.m_pool;
        m_set = other.m_set;
        m_sampler = other.m_sampler;
        m_imageView1 = other.m_imageView1;
        m_imageView1 = other.m_imageView2;
        m_imageLayout = other.m_imageLayout;
        m_uniformBuffer = std::move(other.m_uniformBuffer);

        other.m_pool = nullptr;
        other.m_set = VK_NULL_HANDLE;
        other.m_sampler = VK_NULL_HANDLE;
        other.m_imageView1 = nullptr;
        other.m_imageView2 = nullptr;
        other.m_imageLayout = VK_IMAGE_LAYOUT_UNDEFINED;
        other.m_uniformBuffer = nullptr;
        return *this;
    }

private:
    SceneDescriptorPool *m_pool;
    VkDescriptorSet m_set;
    VkSampler m_sampler;
    std::shared_ptr<VulkanImageView> m_imageView1;
    std::shared_ptr<VulkanImageView> m_imageView2;
    VkImageLayout m_imageLayout;
    std::shared_ptr<VulkanBuffer> m_uniformBuffer;
};



// ------------------------------------------------------------------



/**
 * ColorDescriptorSet encapsulates a descriptor set with a dynamic uniform buffer binding.
 */
class ColorDescriptorSet : public VulkanObject
{
public:
    ColorDescriptorSet(SceneDescriptorPool *pool);

    ColorDescriptorSet(const ColorDescriptorSet &other) = delete;

    ColorDescriptorSet(ColorDescriptorSet &&other)
        : m_pool(other.m_pool),
          m_set(other.m_set),
          m_uniformBuffer(std::move(other.m_uniformBuffer))
    {
        other.m_pool = nullptr;
        other.m_set = VK_NULL_HANDLE;
        other.m_uniformBuffer = nullptr;
    }

    ~ColorDescriptorSet() override;

    void update(const std::shared_ptr<VulkanBuffer> &uniformBuffer);

    std::shared_ptr<VulkanBuffer> uniformBuffer() const { return m_uniformBuffer; }

    VkDescriptorSet handle() const { return m_set; }
    operator VkDescriptorSet () const { return m_set; }

    ColorDescriptorSet &operator = (const ColorDescriptorSet &other) = delete;

    ColorDescriptorSet &operator = (ColorDescriptorSet &&other) {
        m_pool = other.m_pool;
        m_set = other.m_set;
        m_uniformBuffer = std::move(other.m_uniformBuffer);

        other.m_pool = nullptr;
        other.m_set = VK_NULL_HANDLE;
        other.m_uniformBuffer = nullptr;
        return *this;
    }

private:
    SceneDescriptorPool *m_pool;
    VkDescriptorSet m_set;
    std::shared_ptr<VulkanBuffer> m_uniformBuffer;
};



// ------------------------------------------------------------------



/**
 * SceneDescriptorPool allocates descriptors for descriptor sets.
 */
class SceneDescriptorPool
{
public:
    struct CreateInfo {
        VulkanDevice *device;
        VkDescriptorSetLayout descriptorSetLayout;
        uint32_t maxSets;
        std::initializer_list<VkDescriptorPoolSize> poolSizes;
    };

    SceneDescriptorPool(const CreateInfo &createInfo);
    ~SceneDescriptorPool();

    VulkanDevice *device() const { return m_device; }

private:
    VkDescriptorSet allocateDescriptorSet();
    void freeDescriptorSet(VkDescriptorSet set);

private:
    VulkanDevice *m_device;
    std::deque<VkDescriptorPool> m_pools;
    std::queue<VkDescriptorSet> m_unusedSets;
    std::vector<VkDescriptorPoolSize> m_poolSizes;
    VkDescriptorSetLayout m_layout = VK_NULL_HANDLE;
    uint32_t m_maxSets = 0;
    uint32_t m_available = 0;

friend class TextureDescriptorSet;
friend class CrossFadeDescriptorSet;
friend class ColorDescriptorSet;
friend class DecorationImagesDescriptorSet;
};

} // namespace KWin

#endif // DESCRIPTOR_SET_H

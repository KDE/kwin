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

#include "descriptorset.h"

namespace KWin {

#define ARRAY_SIZE(a) (sizeof(a) / sizeof(a[0]))


SceneDescriptorPool::SceneDescriptorPool(const CreateInfo &createInfo)
    : m_device(createInfo.device),
      m_poolSizes(createInfo.poolSizes),
      m_layout(createInfo.descriptorSetLayout),
      m_maxSets(createInfo.maxSets)
{
}


SceneDescriptorPool::~SceneDescriptorPool()
{
    for (VkDescriptorPool pool : m_pools)
        m_device->destroyDescriptorPool(pool, nullptr);
}


VkDescriptorSet SceneDescriptorPool::allocateDescriptorSet()
{
    if (m_unusedSets.size() > 0) {
        const VkDescriptorSet set = m_unusedSets.front();
        m_unusedSets.pop();
        return set;
    }

    // Create a new descriptor pool
    if (m_available == 0) {
        // Note that we don't set VK_DESCRIPTOR_POOL_CREATE_FREE_DESCRIPTOR_SET_BIT
        const VkDescriptorPoolCreateInfo createInfo = {
            .sType = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO,
            .pNext = nullptr,
            .flags = 0,
            .maxSets = m_maxSets,
            .poolSizeCount = (uint32_t) m_poolSizes.size(),
            .pPoolSizes = m_poolSizes.data()
        };

        VkDescriptorPool pool;
        m_device->createDescriptorPool(&createInfo, nullptr, &pool);

        m_pools.push_back(pool);
        m_available = m_maxSets;
    }

    // Allocate a new descriptor set from the pool
    const VkDescriptorSetAllocateInfo allocInfo = {
        .sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO,
        .pNext = nullptr,
        .descriptorPool = m_pools.back(),
        .descriptorSetCount = 1,
        .pSetLayouts = &m_layout
    };

    VkDescriptorSet set;
    m_device->allocateDescriptorSets(&allocInfo, &set);

    --m_available;
    return set;
}


void SceneDescriptorPool::freeDescriptorSet(VkDescriptorSet set)
{
    m_unusedSets.push(set);
}



// ------------------------------------------------------------------



TextureDescriptorSet::TextureDescriptorSet(SceneDescriptorPool *pool)
    : m_pool(pool)
{
    m_set = m_pool->allocateDescriptorSet();
}


TextureDescriptorSet::~TextureDescriptorSet()
{
    if (m_pool && m_set)
        m_pool->freeDescriptorSet(m_set);
}


void TextureDescriptorSet::update(VkSampler sampler,
                                      const std::shared_ptr<VulkanImageView> &imageView,
                                      VkImageLayout imageLayout,
                                      const std::shared_ptr<VulkanBuffer> &uniformBuffer)
{
    const VkDescriptorBufferInfo uniformBufferInfo = {
        .buffer = uniformBuffer->handle(),
        .offset = 0,
        .range = sizeof(VulkanPipelineManager::TextureUniformData)
    };

    const VkDescriptorImageInfo imageInfo = {
        .sampler = sampler,
        .imageView = imageView->handle(),
        .imageLayout = imageLayout
    };

    const VkWriteDescriptorSet descriptorWrites[] = {
        {
            .sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET,
            .pNext = nullptr,
            .dstSet = m_set,
            .dstBinding = 0,
            .dstArrayElement = 0,
            .descriptorCount = 1,
            .descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER,
            .pImageInfo = &imageInfo,
            .pBufferInfo = nullptr,
            .pTexelBufferView = nullptr
        },
        {
            .sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET,
            .pNext = nullptr,
            .dstSet = m_set,
            .dstBinding = 1,
            .dstArrayElement = 0,
            .descriptorCount = 1,
            .descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER_DYNAMIC,
            .pImageInfo = nullptr,
            .pBufferInfo = &uniformBufferInfo,
            .pTexelBufferView = nullptr
        },
    };

    m_pool->device()->updateDescriptorSets(ARRAY_SIZE(descriptorWrites), descriptorWrites, 0, nullptr);

    m_sampler = sampler;
    m_imageView = imageView;
    m_imageLayout = imageLayout;
    m_uniformBuffer = uniformBuffer;
}



// ------------------------------------------------------------------



CrossFadeDescriptorSet::CrossFadeDescriptorSet(SceneDescriptorPool *pool)
    : m_pool(pool)
{
    m_set = m_pool->allocateDescriptorSet();
}


CrossFadeDescriptorSet::~CrossFadeDescriptorSet()
{
    if (m_pool && m_set)
        m_pool->freeDescriptorSet(m_set);
}


void CrossFadeDescriptorSet::update(VkSampler sampler,
                                      const std::shared_ptr<VulkanImageView> &imageView1,
                                      const std::shared_ptr<VulkanImageView> &imageView2,
                                      VkImageLayout imageLayout,
                                      const std::shared_ptr<VulkanBuffer> &uniformBuffer)
{
    const VkDescriptorBufferInfo uniformBufferInfo = {
        .buffer = uniformBuffer->handle(),
        .offset = 0,
        .range  = sizeof(VulkanPipelineManager::TextureUniformData)
    };

    const VkDescriptorImageInfo imageInfo[] = {
        {
            .sampler = sampler,
            .imageView = imageView1->handle(),
            .imageLayout = imageLayout
        },
        {
            .sampler = sampler,
            .imageView = imageView2->handle(),
            .imageLayout = imageLayout
        }
    };

    const VkWriteDescriptorSet descriptorWrites[] = {
        {
            .sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET,
            .pNext = nullptr,
            .dstSet = m_set,
            .dstBinding = 0,
            .dstArrayElement = 0,
            .descriptorCount = 2,
            .descriptorType = VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE,
            .pImageInfo = imageInfo,
            .pBufferInfo = nullptr,
            .pTexelBufferView = nullptr
        },
        {
            .sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET,
            .pNext = nullptr,
            .dstSet = m_set,
            .dstBinding = 1,
            .dstArrayElement = 0,
            .descriptorCount = 1,
            .descriptorType = VK_DESCRIPTOR_TYPE_SAMPLER,
            .pImageInfo = imageInfo,
            .pBufferInfo = nullptr,
            .pTexelBufferView = nullptr
        },
        {
            .sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET,
            .pNext = nullptr,
            .dstSet = m_set,
            .dstBinding = 2,
            .dstArrayElement = 0,
            .descriptorCount = 1,
            .descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER_DYNAMIC,
            .pImageInfo = nullptr,
            .pBufferInfo = &uniformBufferInfo,
            .pTexelBufferView = nullptr
        },
    };

    m_pool->device()->updateDescriptorSets(ARRAY_SIZE(descriptorWrites), descriptorWrites, 0, nullptr);

    m_sampler = sampler;
    m_imageView1 = imageView1;
    m_imageView2 = imageView2;
    m_imageLayout = imageLayout;
    m_uniformBuffer = uniformBuffer;
}



// ------------------------------------------------------------------




ColorDescriptorSet::ColorDescriptorSet(SceneDescriptorPool *pool)
    : m_pool(pool)
{
    m_set = m_pool->allocateDescriptorSet();
}


ColorDescriptorSet::~ColorDescriptorSet()
{
    if (m_pool && m_set)
        m_pool->freeDescriptorSet(m_set);
}


void ColorDescriptorSet::update(const std::shared_ptr<VulkanBuffer> &uniformBuffer)
{
    const VkDescriptorBufferInfo uniformBufferInfo = {
        .buffer = uniformBuffer->handle(),
        .offset = 0,
        .range  = sizeof(VulkanPipelineManager::ColorUniformData)
    };

    const VkWriteDescriptorSet descriptorWrite = {
        .sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET,
        .pNext = nullptr,
        .dstSet = m_set,
        .dstBinding = 0,
        .dstArrayElement = 0,
        .descriptorCount = 1,
        .descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER_DYNAMIC,
        .pImageInfo = nullptr,
        .pBufferInfo = &uniformBufferInfo,
        .pTexelBufferView = nullptr
    };

    m_pool->device()->updateDescriptorSets(1, &descriptorWrite, 0, nullptr);

    m_uniformBuffer = uniformBuffer;
}

} // namespace KWin

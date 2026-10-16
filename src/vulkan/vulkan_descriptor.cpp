#include "vulkan_descriptor.h"
#include "vulkan_device.h"
#include "vulkan_texture.h"

namespace KWin
{

VulkanDescriptor::VulkanDescriptor(vk::raii::DescriptorPool &&pool, vk::raii::DescriptorSet &&set, vk::DescriptorType type)
    : m_pool(std::move(pool))
    , m_set(std::move(set))
    , m_type(type)
{
}

const vk::raii::DescriptorSet &VulkanDescriptor::set() const
{
    return m_set;
}

void VulkanDescriptor::write(VulkanTexture *texture)
{
    vk::DescriptorImageInfo info;
    if (m_type == vk::DescriptorType::eSampler || m_type == vk::DescriptorType::eCombinedImageSampler) {
        info.sampler = texture->sampler();
    }
    if (m_type != vk::DescriptorType::eSampler) {
        info.imageView = texture->view();
    }
    info.imageLayout = vk::ImageLayout::eGeneral;

    vk::WriteDescriptorSet write{
        *m_set,
        0,
        0,
        m_type,
        info,
    };
    texture->device()->logicalDevice().updateDescriptorSets(write, {});
}

std::optional<VulkanDescriptor> VulkanDescriptor::allocate(VulkanDevice *device, const vk::raii::DescriptorSetLayout &layout, vk::DescriptorType type, vk::ShaderStageFlags shaderStages)
{
    // FIXME why is pool size 1 not enough???
    vk::DescriptorPoolSize poolSize(type, 5);
    vk::DescriptorPoolCreateInfo poolInfo(vk::DescriptorPoolCreateFlagBits::eFreeDescriptorSet, 1, poolSize);
    auto [imagePoolResult, pool] = device->logicalDevice().createDescriptorPool(poolInfo);
    if (imagePoolResult != vk::Result::eSuccess) {
        return std::nullopt;
    }
    vk::DescriptorSetAllocateInfo info{
        *pool,
        *layout,
    };
    auto [setResult, sets] = device->logicalDevice().allocateDescriptorSets(info);
    if (setResult != vk::Result::eSuccess) {
        return std::nullopt;
    }
    return std::make_optional<VulkanDescriptor>(std::move(pool), std::move(sets.front()), type);
}

}
